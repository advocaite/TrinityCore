/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/// \addtogroup Trinityd
/// @{
/// \file

#include "CliRunnable.h"
#include "CliManager.h"
#include "Config.h"
#include "Util.h"
#include "World.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if TRINITY_PLATFORM != TRINITY_PLATFORM_WINDOWS
#include "Chat.h"
#include "ChatCommand.h"
#include <readline/readline.h>
#include <readline/history.h>
#include <cerrno>
#include <cstdlib>
#include <sys/select.h>
#include <unistd.h>
#else
#include <Windows.h>
#endif

// Always ends with a space: what the user types starts one column after the ">"
static constexpr char CLI_PREFIX[] = "TC> ";

/*
 * How the console prompt works
 * ----------------------------
 * The prompt ("TC> " plus whatever the user has typed so far) lives on the last row of the screen. Everything
 * else that prints to the console (log lines, command output) does so inside a CliManager::OutputGuard.
 * The guard calls Suspend() on the object that owns the prompt, which removes the prompt and the typed text
 * from the screen. After the output has been printed, Resume() draws "TC> <typed text>" again on the row below it
 * and puts the cursor back where it was inside the text.
 *
 * Two prompt owners exist:
 *  - ReadlineInput (Linux): GNU readline's callback interface
 *  - WindowsInput (Windows): reads key presses and lets a LineEditor draw the line with escape sequences
 *
 * No attempt is made to find out what kind of console this is: the terminal commands are always sent.
 */

namespace Trinity::Impl::Cli
{
    using StopRequested = std::function<bool()>;
    using LineHandler = std::function<void(std::string&&)>;

    // ---------------------------------------------------------------------------------------------------------
    // Single line editor, draws "<prompt><text>" with VT100/ANSI escape sequences and knows where everything is on
    // screen (including rows the text wraps to). Used on Windows, where there is no readline.
    // The editor does not read input itself: the platform code translates key presses into Key values and passes
    // them to Feed(), which returns the lines that were finished with Enter.
    // ---------------------------------------------------------------------------------------------------------
    class LineEditor final : public CliManager::Handler
    {
    public:
        enum class KeyType : uint8
        {
            Char,           // insert Key::Ch
            Enter,
            Backspace,
            Delete,
            Left,
            Right,
            WordLeft,
            WordRight,
            Up,             // previous history entry
            Down,           // next history entry
            Home,
            End,
            KillToStart,    // Ctrl+U
            KillToEnd,      // Ctrl+K
            KillWord,       // Ctrl+W
            ClearLine       // Escape
        };

        struct Key
        {
            KeyType Type = KeyType::Char;
            char32_t Ch = 0;
        };

        /// Writes UTF-8 text to the console
        using Writer = std::function<void(std::string_view)>;
        /// Returns the number of columns after which the console wraps to the next row
        using WidthProvider = std::function<uint32()>;

        LineEditor(std::string prompt, Writer writer, WidthProvider widthProvider)
            : _prompt(std::move(prompt)), _writer(std::move(writer)), _widthProvider(std::move(widthProvider))
        {
        }

        /// Draws the prompt. The cursor has to be at the start of an empty row
        void Show()
        {
            std::lock_guard<std::recursive_mutex> lock(CliManager::GetMutex());
            _active = true;
            _text.clear();
            _cursor = 0;

            std::string out;
            AppendDraw(out);
            _writer(out);
        }

        /// Removes the prompt and typed text from the screen (shutdown)
        void Hide()
        {
            std::lock_guard<std::recursive_mutex> lock(CliManager::GetMutex());
            if (_active && _visible)
            {
                std::string out;
                AppendErase(out);
                _writer(out);
            }

            _active = false;
            _resumeNeeded = false;
        }

        /// Applies key presses and redraws once. Returns the lines that were finished with Enter
        std::vector<std::string> Feed(Key const* keys, size_t count)
        {
            std::vector<std::string> lines;

            std::lock_guard<std::recursive_mutex> lock(CliManager::GetMutex());
            if (!_active)
                return lines;

            std::string out;
            for (size_t i = 0; i < count; ++i)
            {
                if (keys[i].Type != KeyType::Enter)
                {
                    Apply(keys[i]);
                    continue;
                }

                // if something was typed since the last draw, the screen needs to show the final text before the line is committed
                if (_dirty || !_visible)
                    AppendRefresh(out);

                AppendCommit(out);
                lines.push_back(ToUtf8(_text));
                HistoryAdd(_text);
                _text.clear();
                _cursor = 0;

                // new, empty prompt right below
                AppendDraw(out);
            }

            if (_dirty)
                AppendRefresh(out);

            if (!out.empty())
                _writer(out);

            return lines;
        }

        // CliManager::Handler
        void Suspend() override
        {
            std::lock_guard<std::recursive_mutex> lock(CliManager::GetMutex());
            _resumeNeeded = _active && _visible;
            if (_resumeNeeded)
            {
                std::string out;
                AppendErase(out);
                _writer(out);
            }
        }

        void Resume() override
        {
            std::lock_guard<std::recursive_mutex> lock(CliManager::GetMutex());
            if (_resumeNeeded && _active)
            {
                std::string out;
                AppendDraw(out);
                _writer(out);
            }

            _resumeNeeded = false;
        }

    private:
        static constexpr size_t MAX_LINE_LENGTH = 4096;
        static constexpr size_t MAX_HISTORY_SIZE = 500;
        static constexpr uint32 MIN_CONSOLE_WIDTH = 16;
        static constexpr uint32 DEFAULT_CONSOLE_WIDTH = 80;

        struct Position
        {
            uint32 Row;
            uint32 Col;
        };

        static void AppendUtf8(std::string& out, char32_t c)
        {
            if (c < 0x80)
                out += static_cast<char>(c);
            else if (c < 0x800)
            {
                out += static_cast<char>(0xC0 | (c >> 6));
                out += static_cast<char>(0x80 | (c & 0x3F));
            }
            else if (c < 0x10000)
            {
                out += static_cast<char>(0xE0 | (c >> 12));
                out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (c & 0x3F));
            }
            else
            {
                out += static_cast<char>(0xF0 | (c >> 18));
                out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (c & 0x3F));
            }
        }

        static std::string ToUtf8(std::u32string const& str)
        {
            std::string out;
            out.reserve(str.size());
            for (char32_t c : str)
                AppendUtf8(out, c);
            return out;
        }

        // number of console cells a character occupies (0 for combining characters, 2 for east asian wide characters and emoji)
        static uint32 CharWidth(char32_t c)
        {
            if ((c >= 0x0300 && c <= 0x036F) || (c >= 0x200B && c <= 0x200F) || (c >= 0xFE00 && c <= 0xFE0F) || c == 0xFEFF)
                return 0;

            if ((c >= 0x1100 && c <= 0x115F) || (c >= 0x2E80 && c <= 0xA4CF) || (c >= 0xAC00 && c <= 0xD7A3) ||
                (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFE30 && c <= 0xFE6F) || (c >= 0xFF00 && c <= 0xFF60) ||
                (c >= 0xFFE0 && c <= 0xFFE6) || (c >= 0x1F300 && c <= 0x1F64F) || (c >= 0x1F900 && c <= 0x1F9FF) ||
                (c >= 0x20000 && c <= 0x3FFFD))
                return 2;

            return 1;
        }

        static void AppendCsi(std::string& out, uint32 n, char command)
        {
            out += "\x1b[";
            out += std::to_string(n);
            out += command;
        }

        uint32 GetWidth() const
        {
            uint32 width = _widthProvider ? _widthProvider() : DEFAULT_CONSOLE_WIDTH;
            if (!width)
                width = DEFAULT_CONSOLE_WIDTH;
            return std::max(width, MIN_CONSOLE_WIDTH);
        }

        // Where the terminal cursor is (relative to the first character of the prompt) when it stands in front of text[index]
        Position PositionOf(size_t index, uint32 width) const
        {
            uint32 row = 0;
            uint32 col = static_cast<uint32>(_prompt.size());

            for (size_t i = 0; i < index; ++i)
            {
                uint32 w = CharWidth(_text[i]);
                if (!w)
                    continue;

                if (col + w > width)    // a wide character does not fit at the end of the row and moves to the next one
                {
                    ++row;
                    col = 0;
                }

                col += w;
                if (col >= width)       // row is full, the next character starts on the next row
                {
                    ++row;
                    col = 0;
                }
            }

            if (index < _text.size())
            {
                uint32 w = CharWidth(_text[index]);
                if (w && col + w > width)
                {
                    ++row;
                    col = 0;
                }
            }

            return { row, col };
        }

        // moves to the start of the row the prompt began on and clears everything from there to the end of the screen
        void AppendErase(std::string& out)
        {
            if (_cursorRow)
                AppendCsi(out, _cursorRow, 'A');

            out += "\r\x1b[J";
            _cursorRow = 0;
            _visible = false;
        }

        // draws prompt and text at the cursor (start of an empty row) and moves the cursor to its place inside the text
        void AppendDraw(std::string& out)
        {
            uint32 width = GetWidth();

            out += _prompt;
            out += ToUtf8(_text);

            Position end = PositionOf(_text.size(), width);
            if (end.Col == 0 && end.Row > 0)
                out += " \r";   // the text ends exactly at the right edge: force the terminal to really move to the next row, whether it wraps immediately or delays the wrap

            Position cur = PositionOf(_cursor, width);
            if (end.Row > cur.Row)
                AppendCsi(out, end.Row - cur.Row, 'A');
            AppendCsi(out, cur.Col + 1, 'G');

            _cursorRow = cur.Row;
            _drawnWidth = width;
            _visible = true;
            _dirty = false;
        }

        void AppendRefresh(std::string& out)
        {
            if (_visible)
                AppendErase(out);
            AppendDraw(out);
        }

        // leaves the line on screen as it is and moves to the row below it
        void AppendCommit(std::string& out)
        {
            Position end = PositionOf(_text.size(), _drawnWidth);
            if (end.Row > _cursorRow)
                AppendCsi(out, end.Row - _cursorRow, 'B');

            out += (end.Col != 0) ? "\r\n" : "\r";
            _cursorRow = 0;
            _visible = false;
        }

        void HistoryAdd(std::u32string const& line)
        {
            if (!line.empty() && (_history.empty() || _history.back() != line))
            {
                _history.push_back(line);
                if (_history.size() > MAX_HISTORY_SIZE)
                    _history.erase(_history.begin());
            }

            _historyPos = _history.size();
            _draft.clear();
        }

        void Apply(Key const& key)
        {
            auto isSpace = [](char32_t c) { return c == U' '; };

            switch (key.Type)
            {
                case KeyType::Char:
                    if (key.Ch < 0x20 || (key.Ch >= 0x7F && key.Ch < 0xA0) || _text.size() >= MAX_LINE_LENGTH)
                        return;
                    _text.insert(_text.begin() + _cursor, key.Ch);
                    ++_cursor;
                    break;
                case KeyType::Backspace:
                    if (!_cursor)
                        return;
                    _text.erase(_cursor - 1, 1);
                    --_cursor;
                    break;
                case KeyType::Delete:
                    if (_cursor >= _text.size())
                        return;
                    _text.erase(_cursor, 1);
                    break;
                case KeyType::Left:
                    if (!_cursor)
                        return;
                    --_cursor;
                    break;
                case KeyType::Right:
                    if (_cursor >= _text.size())
                        return;
                    ++_cursor;
                    break;
                case KeyType::WordLeft:
                {
                    size_t j = _cursor;
                    while (j > 0 && isSpace(_text[j - 1]))
                        --j;
                    while (j > 0 && !isSpace(_text[j - 1]))
                        --j;
                    if (j == _cursor)
                        return;
                    _cursor = j;
                    break;
                }
                case KeyType::WordRight:
                {
                    size_t j = _cursor;
                    while (j < _text.size() && isSpace(_text[j]))
                        ++j;
                    while (j < _text.size() && !isSpace(_text[j]))
                        ++j;
                    if (j == _cursor)
                        return;
                    _cursor = j;
                    break;
                }
                case KeyType::Home:
                    if (!_cursor)
                        return;
                    _cursor = 0;
                    break;
                case KeyType::End:
                    if (_cursor == _text.size())
                        return;
                    _cursor = _text.size();
                    break;
                case KeyType::KillToStart:
                    if (!_cursor)
                        return;
                    _text.erase(0, _cursor);
                    _cursor = 0;
                    break;
                case KeyType::KillToEnd:
                    if (_cursor >= _text.size())
                        return;
                    _text.erase(_cursor);
                    break;
                case KeyType::KillWord:
                {
                    size_t j = _cursor;
                    while (j > 0 && isSpace(_text[j - 1]))
                        --j;
                    while (j > 0 && !isSpace(_text[j - 1]))
                        --j;
                    if (j == _cursor)
                        return;
                    _text.erase(j, _cursor - j);
                    _cursor = j;
                    break;
                }
                case KeyType::ClearLine:
                    if (_text.empty())
                        return;
                    _text.clear();
                    _cursor = 0;
                    break;
                case KeyType::Up:
                    if (!_historyPos)
                        return;
                    if (_historyPos == _history.size())
                        _draft = _text;
                    --_historyPos;
                    _text = _history[_historyPos];
                    _cursor = _text.size();
                    break;
                case KeyType::Down:
                    if (_historyPos >= _history.size())
                        return;
                    ++_historyPos;
                    _text = _historyPos == _history.size() ? _draft : _history[_historyPos];
                    _cursor = _text.size();
                    break;
                case KeyType::Enter:
                    return; // handled by Feed
            }

            _dirty = true;
        }

        std::string _prompt;
        Writer _writer;
        WidthProvider _widthProvider;

        std::u32string _text;
        size_t _cursor = 0;

        bool _active = false;       // between Show() and Hide()
        bool _resumeNeeded = false; // Suspend() took the prompt off the screen and Resume() has to bring it back
        bool _visible = false;      // prompt and text are on screen
        uint32 _cursorRow = 0;      // row of the terminal cursor, relative to the row the prompt starts on (only valid while _visible)
        uint32 _drawnWidth = 0;     // console width used for the last draw
        bool _dirty = false;        // the text changed and the screen does not show it yet

        std::vector<std::u32string> _history;
        size_t _historyPos = 0;
        std::u32string _draft;
    };

#if TRINITY_PLATFORM != TRINITY_PLATFORM_WINDOWS
    // ---------------------------------------------------------------------------------------------------------
    // Console input based on GNU readline's callback interface.
    // Using the callback interface (instead of the blocking readline() call) keeps all readline calls on one thread
    // under the prompt mutex, which makes it possible for other threads to take the prompt and the half-typed
    // command off the screen while they print (Suspend) and to put them back below their output afterwards (Resume).
    // ---------------------------------------------------------------------------------------------------------
    class ReadlineInput final : public CliManager::Handler
    {
    public:
        using EofHandler = std::function<void()>;

        explicit ReadlineInput(char const* prompt) : _prompt(prompt) { }
        ~ReadlineInput() { free(_savedText); }

        ReadlineInput(ReadlineInput const&) = delete;
        ReadlineInput& operator=(ReadlineInput const&) = delete;

        /// Shows the prompt and handles input until stopRequested returns true
        void Run(StopRequested const& stopRequested, LineHandler lineHandler, EofHandler eofHandler)
        {
            _lineHandler = std::move(lineHandler);
            _eofHandler = std::move(eofHandler);
            _instance = this;

            {
                std::lock_guard<std::recursive_mutex> lock(CliManager::GetMutex());
                rl_callback_handler_install(_prompt.c_str(), &ReadlineInput::OnLine);
                CliManager::SetHandler(this);
            }

            while (!stopRequested())
            {
                if (_inputClosed)
                {
                    // stdin is gone for good (e.g. started as a service), keep running without a console
                    timeval tv = { 0, 100000 };
                    select(0, nullptr, nullptr, nullptr, &tv);
                    continue;
                }

                // wake up regularly so the loop notices that the server is shutting down
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(STDIN_FILENO, &fds);
                timeval tv = { 0, 100000 };
                int ready = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
                if (ready < 0)
                {
                    if (errno != EINTR)
                        _inputClosed = true;
                    continue;
                }

                if (ready > 0)
                {
                    std::lock_guard<std::recursive_mutex> lock(CliManager::GetMutex());
                    rl_callback_read_char();
                }
            }

            std::lock_guard<std::recursive_mutex> lock(CliManager::GetMutex());
            CliManager::SetHandler(nullptr);
            rl_callback_handler_remove(); // gives the terminal back in its original mode
            _instance = nullptr;
        }

        // CliManager::Handler, called by CliManager::OutputGuard with the prompt mutex held
        void Suspend() override
        {
            free(_savedText);
            _savedPoint = rl_point;
            _savedText = rl_copy_text(0, rl_end);

            // make readline draw an empty line without prompt: that removes "TC> <text>" from the screen
            // (including the extra rows if it was wrapped) and leaves the cursor at the start of the row
            rl_set_prompt("");
            rl_replace_line("", 0);
            rl_redisplay();
            _suspended = true;
        }

        void Resume() override
        {
            if (!_suspended)
                return;

            // the cursor is at the start of an empty row below the output: draw the prompt and the remembered text again
            rl_set_prompt(_prompt.c_str());
            rl_replace_line(_savedText ? _savedText : "", 0);
            rl_point = _savedPoint;
            rl_redisplay();

            free(_savedText);
            _savedText = nullptr;
            _suspended = false;
        }

    private:
        static void OnLine(char* line)
        {
            // runs inside rl_callback_read_char(), the prompt mutex is held. Nothing in here may print or log
            ReadlineInput* self = _instance;
            if (!line)
            {
                // Ctrl+D on a terminal: ignored. End of input on a pipe or file: there is nothing more to read,
                // stop polling stdin (it would report "readable" forever)
                if (!isatty(STDIN_FILENO))
                    self->_inputClosed = true;
                if (self->_eofHandler)
                    self->_eofHandler();
                return;
            }

            if (*line)
            {
                add_history(line);
                if (self->_lineHandler)
                    self->_lineHandler(std::string(line));
            }

            free(line);
        }

        static inline ReadlineInput* _instance = nullptr; // readline is a singleton too

        std::string _prompt;
        LineHandler _lineHandler;
        EofHandler _eofHandler;
        bool _inputClosed = false;

        // what Suspend() took away
        char* _savedText = nullptr;
        int _savedPoint = 0;
        bool _suspended = false;
    };

    static std::vector<std::string> _completions;

    static char* cli_unpack_vector(char const*, int state)
    {
        static size_t i = 0;
        if (!state)
            i = 0;
        if (i < _completions.size())
            return strdup(_completions[i++].c_str());
        else
            return nullptr;
    }

    static char** cli_completion(char const* text, int /*start*/, int /*end*/)
    {
        ::rl_attempted_completion_over = 1;
        _completions = Trinity::ChatCommands::GetAutoCompletionsFor(CliHandler(nullptr, nullptr), text);
        return ::rl_completion_matches(text, &cli_unpack_vector);
    }

#else
    // ---------------------------------------------------------------------------------------------------------
    // Console input for the Windows console: reads key presses directly (instead of letting the console collect a
    // whole line) and feeds them to a LineEditor.
    // ---------------------------------------------------------------------------------------------------------
    class WindowsInput
    {
    public:
        explicit WindowsInput(char const* prompt)
            : _editor(prompt, [](std::string_view text) { WriteWinConsole(text); }, &GetConsoleWidth)
        {
        }

        void Run(StopRequested const& stopRequested, LineHandler const& lineHandler)
        {
            HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
            HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

            // switch to key-by-key input, no echo (the editor draws the typed text itself), escape sequences on output.
            // Ctrl+C stays a signal. Failures are not fatal, the commands are sent regardless
            DWORD inMode = 0, outMode = 0;
            bool inputIsConsole = GetConsoleMode(in, &inMode) != 0;
            bool outputIsConsole = GetConsoleMode(out, &outMode) != 0;
            if (outputIsConsole)
                SetConsoleMode(out, outMode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            if (inputIsConsole)
                SetConsoleMode(in, (inMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT)) | ENABLE_PROCESSED_INPUT);

            {
                std::lock_guard<std::recursive_mutex> lock(CliManager::GetMutex());
                _editor.Show();
                CliManager::SetHandler(&_editor);
            }

            std::vector<LineEditor::Key> keys;
            INPUT_RECORD records[64];

            while (!stopRequested())
            {
                if (!inputIsConsole)
                {
                    // nothing to read key presses from (input is redirected), keep the server running
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                // wake up regularly so the loop notices that the server is shutting down
                DWORD wait = WaitForSingleObject(in, 100);
                if (wait == WAIT_TIMEOUT)
                    continue;
                if (wait != WAIT_OBJECT_0)
                {
                    inputIsConsole = false;
                    continue;
                }

                DWORD available = 0;
                if (!GetNumberOfConsoleInputEvents(in, &available))
                {
                    inputIsConsole = false;
                    continue;
                }
                if (!available)
                    continue;

                DWORD count = 0;
                if (!ReadConsoleInputW(in, records, static_cast<DWORD>(std::size(records)), &count))
                {
                    inputIsConsole = false;
                    continue;
                }

                keys.clear();
                for (DWORD i = 0; i < count; ++i)
                    TranslateRecord(records[i], keys);

                if (keys.empty())
                    continue;

                for (std::string& line : _editor.Feed(keys.data(), keys.size()))
                    if (!line.empty())
                        lineHandler(std::move(line));
            }

            {
                std::lock_guard<std::recursive_mutex> lock(CliManager::GetMutex());
                CliManager::SetHandler(nullptr);
                _editor.Hide();
            }

            if (inputIsConsole)
                SetConsoleMode(in, inMode);
            if (outputIsConsole)
                SetConsoleMode(out, outMode);
        }

    private:
        static uint32 GetConsoleWidth()
        {
            CONSOLE_SCREEN_BUFFER_INFO info;
            if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info) && info.dwSize.X > 0)
                return static_cast<uint32>(info.dwSize.X); // lines wrap at the width of the screen buffer, not of the window

            return 0;
        }

        void TranslateRecord(INPUT_RECORD const& record, std::vector<LineEditor::Key>& keys)
        {
            using KeyType = LineEditor::KeyType;

            if (record.EventType != KEY_EVENT)
                return;

            KEY_EVENT_RECORD const& event = record.Event.KeyEvent;
            if (!event.bKeyDown)
                return;

            bool ctrl = (event.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
            bool altGr = ctrl && (event.dwControlKeyState & RIGHT_ALT_PRESSED);
            WCHAR ch = event.uChar.UnicodeChar;
            uint32 repeat = std::max<uint32>(event.wRepeatCount, 1);

            Optional<KeyType> special;
            switch (event.wVirtualKeyCode)
            {
                case VK_RETURN:     special = KeyType::Enter; break;
                case VK_BACK:       special = ctrl ? KeyType::KillWord : KeyType::Backspace; break;
                case VK_DELETE:     special = KeyType::Delete; break;
                case VK_LEFT:       special = ctrl ? KeyType::WordLeft : KeyType::Left; break;
                case VK_RIGHT:      special = ctrl ? KeyType::WordRight : KeyType::Right; break;
                case VK_UP:         special = KeyType::Up; break;
                case VK_DOWN:       special = KeyType::Down; break;
                case VK_HOME:       special = KeyType::Home; break;
                case VK_END:        special = KeyType::End; break;
                case VK_ESCAPE:     special = KeyType::ClearLine; break;
                default:
                    break;
            }

            if (special)
            {
                for (uint32 n = 0; n < repeat; ++n)
                    keys.push_back({ *special, 0 });
                return;
            }

            if (event.wVirtualKeyCode == VK_TAB)
                return;

            if (ch == L'\r')    // line break in pasted text
            {
                keys.push_back({ KeyType::Enter, 0 });
                return;
            }

            if (ctrl && !altGr && ch >= 1 && ch <= 26)
            {
                // Ctrl+<letter> arrives as control code
                Optional<KeyType> control;
                switch (ch)
                {
                    case 1:  control = KeyType::Home; break;        // Ctrl+A
                    case 5:  control = KeyType::End; break;         // Ctrl+E
                    case 11: control = KeyType::KillToEnd; break;   // Ctrl+K
                    case 21: control = KeyType::KillToStart; break; // Ctrl+U
                    case 23: control = KeyType::KillWord; break;    // Ctrl+W
                    default:
                        break;
                }

                if (control)
                    keys.push_back({ *control, 0 });
                return;
            }

            if (ch < 0x20 || ch == 0x7F)
                return;

            char32_t codePoint = ch;
            if (ch >= 0xD800 && ch <= 0xDBFF)
            {
                _highSurrogate = ch;
                return;
            }
            if (ch >= 0xDC00 && ch <= 0xDFFF)
            {
                if (!_highSurrogate)
                    return;

                codePoint = 0x10000 + ((static_cast<char32_t>(_highSurrogate) - 0xD800) << 10) + (static_cast<char32_t>(ch) - 0xDC00);
                _highSurrogate = 0;
            }

            for (uint32 n = 0; n < repeat; ++n)
                keys.push_back({ KeyType::Char, codePoint });
        }

        LineEditor _editor;
        char16_t _highSurrogate = 0;
    };
#endif
}

// Writes text to the console. The prompt (with whatever is typed after it) is taken off the screen
// while the text is printed and drawn again below it, so the text always starts on a clean row
static void WriteCliOutput(std::string_view str)
{
    CliManager::OutputGuard promptGuard;

#if TRINITY_PLATFORM == TRINITY_PLATFORM_WINDOWS
    WriteWinConsole(str);
#else
    fwrite(str.data(), 1, str.size(), stdout);
    fflush(stdout);
#endif
}

// Command output arrives in pieces (a message and its line break are sent separately) from the world thread.
// Only complete lines are written, so the prompt can never end up in the middle of a line
static std::string _cliOutputBuffer;

void utf8print(void* /*arg*/, std::string_view str)
{
    _cliOutputBuffer.append(str);

    size_t lastLineEnd = _cliOutputBuffer.rfind('\n');
    if (lastLineEnd == std::string::npos)
        return;

    WriteCliOutput(std::string_view(_cliOutputBuffer).substr(0, lastLineEnd + 1));
    _cliOutputBuffer.erase(0, lastLineEnd + 1);
}

void commandFinished(void*, bool /*success*/)
{
    // output of the command that did not end with a line break
    if (!_cliOutputBuffer.empty())
    {
        _cliOutputBuffer.push_back('\n');
        WriteCliOutput(_cliOutputBuffer);
        _cliOutputBuffer.clear();
    }
}

#ifdef linux
// Non-blocking keypress detector, when return pressed, return 1, else always return 0
int kb_hit_return()
{
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO+1, &fds, nullptr, nullptr, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}
#endif

static void SubmitCliCommand(std::string&& command)
{
    Optional<std::size_t> nextLineIndex = RemoveCRLF(command);
    if (nextLineIndex && *nextLineIndex == 0)
        return;

    if (command.empty())
        return;

    sWorld->QueueCliCommand(new CliCommandHolder(nullptr, command.c_str(), &utf8print, &commandFinished));
}

/// %Thread start
void CliThread()
{
#if TRINITY_PLATFORM != TRINITY_PLATFORM_WINDOWS
    ::rl_attempted_completion_function = &Trinity::Impl::Cli::cli_completion;
    {
        static char BLANK = '\0';
        ::rl_completer_word_break_characters = &BLANK;
    }
#endif

    if (sConfigMgr->GetBoolDefault("BeepAtStart", true))
        printf("\a");                                       // \a = Alert

#if TRINITY_PLATFORM == TRINITY_PLATFORM_WINDOWS
    if (sConfigMgr->GetBoolDefault("FlashAtStart", true))
    {
        FLASHWINFO fInfo;
        fInfo.cbSize = sizeof(FLASHWINFO);
        fInfo.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
        fInfo.hwnd = GetConsoleWindow();
        fInfo.uCount = 0;
        fInfo.dwTimeout = 0;
        FlashWindowEx(&fInfo);
    }
#endif

    fflush(stdout);

    ///- As long as the World is running (no World::m_stopEvent), get the command line and handle it
#if TRINITY_PLATFORM == TRINITY_PLATFORM_WINDOWS
    Trinity::Impl::Cli::WindowsInput input(CLI_PREFIX);
    input.Run([] { return World::IsStopped(); }, &SubmitCliCommand);
#else
    Trinity::Impl::Cli::ReadlineInput input(CLI_PREFIX);
    input.Run([] { return World::IsStopped(); }, &SubmitCliCommand, [] { if (feof(stdin)) World::StopNow(SHUTDOWN_EXIT_CODE); });
#endif
}

/// @}
