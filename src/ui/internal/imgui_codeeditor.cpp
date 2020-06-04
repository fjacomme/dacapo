#include "ui/internal/imgui_codeeditor.h"
#pragma warning(disable : 4505)
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"

using namespace ImGui;
using namespace ImStb;
static bool   InputTextFilterCharacter(unsigned int*          p_char,
                                       ImGuiInputTextFlags    flags,
                                       ImGuiInputTextCallback callback,
                                       void*                  user_data);
static int    InputTextCalcTextLenAndLineCount(const char* text_begin, const char** out_text_end);
static ImVec2 InputTextCalcTextSizeW(const ImWchar*  text_begin,
                                     const ImWchar*  text_end,
                                     const ImWchar** remaining        = NULL,
                                     ImVec2*         out_offset       = NULL,
                                     bool            stop_on_new_line = false);

static int InputTextCalcTextLenAndLineCount(const char* text_begin, const char** out_text_end)
{
    int         line_count = 0;
    const char* s          = text_begin;
    while (char c = *s++) // We are only matching for \n so we can ignore UTF-8 decoding
        if (c == '\n')
            line_count++;
    s--;
    if (s[0] != '\n' && s[0] != '\r')
        line_count++;
    *out_text_end = s;
    return line_count;
}

static ImVec2 InputTextCalcTextSizeW(const ImWchar*  text_begin,
                                     const ImWchar*  text_end,
                                     const ImWchar** remaining,
                                     ImVec2*         out_offset,
                                     bool            stop_on_new_line)
{
    ImGuiContext& g           = *GImGui;
    ImFont*       font        = g.Font;
    const float   line_height = g.FontSize;
    const float   scale       = line_height / font->FontSize;

    ImVec2 text_size  = ImVec2(0, 0);
    float  line_width = 0.0f;

    const ImWchar* s = text_begin;
    while (s < text_end) {
        unsigned int c = (unsigned int)(*s++);
        if (c == '\n') {
            text_size.x = ImMax(text_size.x, line_width);
            text_size.y += line_height;
            line_width = 0.0f;
            if (stop_on_new_line)
                break;
            continue;
        }
        if (c == '\r')
            continue;

        const float char_width = font->GetCharAdvance((ImWchar)c) * scale;
        line_width += char_width;
    }

    if (text_size.x < line_width)
        text_size.x = line_width;

    if (out_offset)
        *out_offset = ImVec2(
            line_width,
            text_size.y
                + line_height); // offset allow for the possibility of sitting after a trailing \n

    if (line_width > 0 || text_size.y == 0.0f) // whereas size.y will ignore the trailing \n
        text_size.y += line_height;

    if (remaining)
        *remaining = s;

    return text_size;
}

// Wrapper for stb_textedit.h to edit text (our wrapper is for: statically sized buffer,
// single-line, wchar characters. InputText converts between UTF-8 and wchar)
namespace ImStb {
static int STB_TEXTEDIT_STRINGLEN(const STB_TEXTEDIT_STRING* obj)
{
    return obj->CurLenW;
}
static ImWchar STB_TEXTEDIT_GETCHAR(const STB_TEXTEDIT_STRING* obj, int idx)
{
    return obj->TextW[idx];
}
static float STB_TEXTEDIT_GETWIDTH(STB_TEXTEDIT_STRING* obj, int line_start_idx, int char_idx)
{
    ImWchar c = obj->TextW[line_start_idx + char_idx];
    if (c == '\n')
        return STB_TEXTEDIT_GETWIDTH_NEWLINE;
    ImGuiContext& g = *GImGui;
    return g.Font->GetCharAdvance(c) * (g.FontSize / g.Font->FontSize);
}
static int STB_TEXTEDIT_KEYTOTEXT(int key)
{
    return key >= 0x200000 ? 0 : key;
}
static ImWchar STB_TEXTEDIT_NEWLINE = '\n';
static void STB_TEXTEDIT_LAYOUTROW(StbTexteditRow* r, STB_TEXTEDIT_STRING* obj, int line_start_idx)
{
    const ImWchar* text           = obj->TextW.Data;
    const ImWchar* text_remaining = NULL;
    const ImVec2   size = InputTextCalcTextSizeW(text + line_start_idx, text + obj->CurLenW,
                                               &text_remaining, NULL, true);
    r->x0               = 0.0f;
    r->x1               = size.x;
    r->baseline_y_delta = size.y;
    r->ymin             = 0.0f;
    r->ymax             = size.y;
    r->num_chars        = (int)(text_remaining - (text + line_start_idx));
}

static bool is_separator(unsigned int c)
{
    return ImCharIsBlankW(c) || c == ',' || c == ';' || c == '(' || c == ')' || c == '{' || c == '}'
        || c == '[' || c == ']' || c == '|';
}
static int is_word_boundary_from_right(STB_TEXTEDIT_STRING* obj, int idx)
{
    return idx > 0 ? (is_separator(obj->TextW[idx - 1]) && !is_separator(obj->TextW[idx])) : 1;
}
static int STB_TEXTEDIT_MOVEWORDLEFT_IMPL(STB_TEXTEDIT_STRING* obj, int idx)
{
    idx--;
    while (idx >= 0 && !is_word_boundary_from_right(obj, idx))
        idx--;
    return idx < 0 ? 0 : idx;
}
#ifdef __APPLE__ // FIXME: Move setting to IO structure
static int is_word_boundary_from_left(STB_TEXTEDIT_STRING* obj, int idx)
{
    return idx > 0 ? (!is_separator(obj->TextW[idx - 1]) && is_separator(obj->TextW[idx])) : 1;
}
static int STB_TEXTEDIT_MOVEWORDRIGHT_IMPL(STB_TEXTEDIT_STRING* obj, int idx)
{
    idx++;
    int len = obj->CurLenW;
    while (idx < len && !is_word_boundary_from_left(obj, idx))
        idx++;
    return idx > len ? len : idx;
}
#else
static int STB_TEXTEDIT_MOVEWORDRIGHT_IMPL(STB_TEXTEDIT_STRING* obj, int idx)
{
    idx++;
    int len = obj->CurLenW;
    while (idx < len && !is_word_boundary_from_right(obj, idx))
        idx++;
    return idx > len ? len : idx;
}
#endif
#define STB_TEXTEDIT_MOVEWORDLEFT \
    STB_TEXTEDIT_MOVEWORDLEFT_IMPL // They need to be #define for stb_textedit.h
#define STB_TEXTEDIT_MOVEWORDRIGHT STB_TEXTEDIT_MOVEWORDRIGHT_IMPL

static void STB_TEXTEDIT_DELETECHARS(STB_TEXTEDIT_STRING* obj, int pos, int n)
{
    ImWchar* dst = obj->TextW.Data + pos;

    // We maintain our buffer length in both UTF-8 and wchar formats
    obj->CurLenA -= ImTextCountUtf8BytesFromStr(dst, dst + n);
    obj->CurLenW -= n;

    // Offset remaining text (FIXME-OPT: Use memmove)
    const ImWchar* src = obj->TextW.Data + pos + n;
    while (ImWchar c = *src++)
        *dst++ = c;
    *dst = '\0';
}

static bool STB_TEXTEDIT_INSERTCHARS(STB_TEXTEDIT_STRING* obj,
                                     int                  pos,
                                     const ImWchar*       new_text,
                                     int                  new_text_len)
{
    const bool is_resizable = (obj->UserFlags & ImGuiInputTextFlags_CallbackResize) != 0;
    const int  text_len     = obj->CurLenW;
    IM_ASSERT(pos <= text_len);

    const int new_text_len_utf8 = ImTextCountUtf8BytesFromStr(new_text, new_text + new_text_len);
    if (!is_resizable && (new_text_len_utf8 + obj->CurLenA + 1 > obj->BufCapacityA))
        return false;

    // Grow internal buffer if needed
    if (new_text_len + text_len + 1 > obj->TextW.Size) {
        if (!is_resizable)
            return false;
        IM_ASSERT(text_len < obj->TextW.Size);
        obj->TextW.resize(text_len + ImClamp(new_text_len * 4, 32, ImMax(256, new_text_len)) + 1);
    }

    ImWchar* text = obj->TextW.Data;
    if (pos != text_len)
        memmove(text + pos + new_text_len, text + pos, (size_t)(text_len - pos) * sizeof(ImWchar));
    memcpy(text + pos, new_text, (size_t)new_text_len * sizeof(ImWchar));

    obj->CurLenW += new_text_len;
    obj->CurLenA += new_text_len_utf8;
    obj->TextW[obj->CurLenW] = '\0';

    return true;
}

// We don't use an enum so we can build even with conflicting symbols (if another user of
// stb_textedit.h leak their STB_TEXTEDIT_K_* symbols)
#define STB_TEXTEDIT_K_LEFT 0x200000      // keyboard input to move cursor left
#define STB_TEXTEDIT_K_RIGHT 0x200001     // keyboard input to move cursor right
#define STB_TEXTEDIT_K_UP 0x200002        // keyboard input to move cursor up
#define STB_TEXTEDIT_K_DOWN 0x200003      // keyboard input to move cursor down
#define STB_TEXTEDIT_K_LINESTART 0x200004 // keyboard input to move cursor to start of line
#define STB_TEXTEDIT_K_LINEEND 0x200005   // keyboard input to move cursor to end of line
#define STB_TEXTEDIT_K_TEXTSTART 0x200006 // keyboard input to move cursor to start of text
#define STB_TEXTEDIT_K_TEXTEND 0x200007   // keyboard input to move cursor to end of text
#define STB_TEXTEDIT_K_DELETE \
    0x200008 // keyboard input to delete selection or character under cursor
#define STB_TEXTEDIT_K_BACKSPACE \
    0x200009 // keyboard input to delete selection or character left of cursor
#define STB_TEXTEDIT_K_UNDO 0x20000A      // keyboard input to perform undo
#define STB_TEXTEDIT_K_REDO 0x20000B      // keyboard input to perform redo
#define STB_TEXTEDIT_K_WORDLEFT 0x20000C  // keyboard input to move cursor left one word
#define STB_TEXTEDIT_K_WORDRIGHT 0x20000D // keyboard input to move cursor right one word
#define STB_TEXTEDIT_K_SHIFT 0x400000

#define STB_TEXTEDIT_IMPLEMENTATION
#include "imstb_textedit.h"

// stb_textedit internally allows for a single undo record to do addition and deletion, but
// somehow, calling the stb_textedit_paste() function creates two separate records, so we
// perform it manually. (FIXME: Report to nothings/stb?)
/*static void stb_textedit_replace(STB_TEXTEDIT_STRING*         str,
                                 STB_TexteditState*           state,
                                 const STB_TEXTEDIT_CHARTYPE* text,
                                 int                          text_len)
{
    stb_text_makeundo_replace(str, state, 0, str->CurLenW, text_len);
    ImStb::STB_TEXTEDIT_DELETECHARS(str, 0, str->CurLenW);
    if (text_len <= 0)
        return;
    if (ImStb::STB_TEXTEDIT_INSERTCHARS(str, 0, text, text_len)) {
        state->cursor          = text_len;
        state->has_preferred_x = 0;
        return;
    }
    IM_ASSERT(0); // Failed to insert character, normally shouldn't happen because of how we
                  // currently use stb_textedit_replace()
}*/

} // namespace ImStb

// Return false to discard a character.
static bool InputTextFilterCharacter(unsigned int*          p_char,
                                     ImGuiInputTextFlags    flags,
                                     ImGuiInputTextCallback callback,
                                     void*                  user_data)
{
    unsigned int c = *p_char;

    // Filter non-printable (NB: isprint is unreliable! see #2467)
    if (c < 0x20) {
        bool pass = false;
        pass |= (c == '\n'); // && (flags & ImGuiInputTextFlags_Multiline));
        pass |= (c == '\t'); // && (flags & ImGuiInputTextFlags_AllowTabInput));
        if (!pass)
            return false;
    }

    // We ignore Ascii representation of delete (emitted from Backspace on OSX, see #2578, #2817)
    if (c == 127)
        return false;

    // Filter private Unicode range. GLFW on OSX seems to send private characters for special keys
    // like arrow keys (FIXME)
    if (c >= 0xE000 && c <= 0xF8FF)
        return false;

    // Filter Unicode ranges we are not handling in this build.
    if (c > IM_UNICODE_CODEPOINT_MAX)
        return false;

    // Generic named filters
    if (flags
        & (ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsHexadecimal
           | ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank
           | ImGuiInputTextFlags_CharsScientific)) {
        if (flags & ImGuiInputTextFlags_CharsDecimal)
            if (!(c >= '0' && c <= '9') && (c != '.') && (c != '-') && (c != '+') && (c != '*')
                && (c != '/'))
                return false;

        if (flags & ImGuiInputTextFlags_CharsScientific)
            if (!(c >= '0' && c <= '9') && (c != '.') && (c != '-') && (c != '+') && (c != '*')
                && (c != '/') && (c != 'e') && (c != 'E'))
                return false;

        if (flags & ImGuiInputTextFlags_CharsHexadecimal)
            if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f') && !(c >= 'A' && c <= 'F'))
                return false;

        if (flags & ImGuiInputTextFlags_CharsUppercase)
            if (c >= 'a' && c <= 'z')
                *p_char = (c += (unsigned int)('A' - 'a'));

        if (flags & ImGuiInputTextFlags_CharsNoBlank)
            if (ImCharIsBlankW(c))
                return false;
    }

    // Custom callback filter
    if (flags & ImGuiInputTextFlags_CallbackCharFilter) {
        ImGuiInputTextCallbackData callback_data;
        memset(&callback_data, 0, sizeof(ImGuiInputTextCallbackData));
        callback_data.EventFlag = ImGuiInputTextFlags_CallbackCharFilter;
        callback_data.EventChar = (ImWchar)c;
        callback_data.Flags     = flags;
        callback_data.UserData  = user_data;
        if (callback(&callback_data) != 0)
            return false;
        *p_char = callback_data.EventChar;
        if (!callback_data.EventChar)
            return false;
    }

    return true;
}

static void render_colored_text(const char*  bgn,
                                const char*  end,
                                ImVec2       startPos,
                                ImU32 const* color_changes)
{
    ImGuiContext& g           = *GImGui;
    ImGuiWindow*  draw_window = GetCurrentWindow();

    const char* buff     = bgn;
    int         line     = 0;
    ImVec2      text_pos = startPos;
    if (!color_changes) {
        auto const col = ImGui::GetColorU32(ImGuiCol_Text);
        draw_window->DrawList->AddText(g.Font, g.FontSize, text_pos, col, bgn, end, 0.0f, NULL);
        return;
    }

    ImU32 col = color_changes ? color_changes[0] : ImGui::GetColorU32(ImGuiCol_Text);
    while (buff != end) {
        auto wb = buff;
        while (buff != end && *buff != '\n' && color_changes[buff - bgn] == col) {
            buff++;
        }
        auto const& newcol = color_changes[buff - bgn];

        draw_window->DrawList->AddText(g.Font, g.FontSize, text_pos, col, wb, buff, 0.0f, NULL);
        auto textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, wb,
                                                        buff, nullptr);
        text_pos.x += textSize.x;
        col = newcol;
        if (buff != end && *buff == '\n') {
            line++;
            buff++;
            text_pos.x = startPos.x;
            text_pos.y += ImGui::GetTextLineHeight();
        }
    }
}

//  doing UTF8 > U16 > UTF8 conversions on the go to easily interface with stb_textedit. Ideally
//  should stay in UTF-8 all the time. See https://github.com/nothings/stb/issues/188)
bool CodeEditor(const char*            name,
                char*                  buf,
                int                    buf_size,
                ImU32 const*           colors,
                const ImVec2&          size_arg,
                ImGuiInputTextFlags    flags,
                ImGuiInputTextCallback callback,
                void*                  callback_user_data)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    flags = ImGuiInputTextFlags_CallbackResize;
    IM_ASSERT(!((flags & ImGuiInputTextFlags_CallbackCompletion)
                && (flags & ImGuiInputTextFlags_AllowTabInput))); // Can't use both together (they
                                                                  // both use tab key)

    const ImGuiID     id    = window->GetID(name);
    ImGuiContext&     g     = *GImGui;
    ImGuiIO&          io    = g.IO;
    const ImGuiStyle& style = g.Style;

    const bool RENDER_SELECTION_WHEN_INACTIVE = false;
    const bool is_readonly                    = (flags & ImGuiInputTextFlags_ReadOnly) != 0;
    const bool is_resizable                   = (flags & ImGuiInputTextFlags_CallbackResize) != 0;
    if (is_resizable)
        IM_ASSERT(callback != NULL); // Must provide a callback if you set the
                                     // ImGuiInputTextFlags_CallbackResize flag!

    BeginGroup();
    const ImVec2 frame_size = CalcItemSize(
        size_arg, CalcItemWidth(),
        (g.FontSize * 8.0f)
            + style.FramePadding.y * 2.0f); // Arbitrary default of 8 lines high for multi-line
    const ImVec2 total_size = ImVec2(frame_size.x + 0.0f, frame_size.y);

    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);
    const ImRect total_bb(frame_bb.Min, frame_bb.Min + total_size);

    ImGuiWindow* draw_window = window;
    ImVec2       inner_size  = frame_size;

    if (!ItemAdd(total_bb, id, &frame_bb)) {
        ItemSize(total_bb, style.FramePadding.y);
        EndGroup();
        return false;
    }
    // We reproduce the contents of BeginChildFrame() in order to provide 'label' so our window
    // internal data are easier to read/debug.
    PushStyleColor(ImGuiCol_ChildBg, style.Colors[ImGuiCol_FrameBg]);
    PushStyleVar(ImGuiStyleVar_ChildRounding, style.FrameRounding);
    PushStyleVar(ImGuiStyleVar_ChildBorderSize, style.FrameBorderSize);
    PushStyleVar(ImGuiStyleVar_WindowPadding, style.FramePadding);
    bool child_visible
        = BeginChildEx(name, id, frame_bb.GetSize(), true,
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysUseWindowPadding);
    PopStyleVar(3);
    PopStyleColor();
    if (!child_visible) {
        EndChild();
        EndGroup();
        return false;
    }
    draw_window = g.CurrentWindow; // Child window
    draw_window->DC.NavLayerActiveMaskNext
        |= draw_window->DC.NavLayerCurrentMask; // This is to ensure that EndChild() will display a
                                                // navigation highlight so we can "enter" into it.
    inner_size.x -= draw_window->ScrollbarSizes.x;

    const bool hovered = ItemHoverable(frame_bb, id);
    if (hovered)
        g.MouseCursor = ImGuiMouseCursor_TextInput;

    // We are only allowed to access the state if we are already the active widget.
    ImGuiInputTextState* state = GetInputTextState(id);

    const bool focus_requested         = FocusableItemRegister(window, id);
    const bool focus_requested_by_code = focus_requested
        && (g.FocusRequestCurrWindow == window
            && g.FocusRequestCurrCounterRegular == window->DC.FocusCounterRegular);
    const bool focus_requested_by_tab = focus_requested && !focus_requested_by_code;

    const bool user_clicked         = hovered && io.MouseClicked[0];
    const bool user_nav_input_start = (g.ActiveId != id)
        && ((g.NavInputId == id)
            || (g.NavActivateId == id && g.NavInputSource == ImGuiInputSource_NavKeyboard));
    const bool user_scroll_finish = state != NULL && g.ActiveId == 0
        && g.ActiveIdPreviousFrame == GetWindowScrollbarID(draw_window, ImGuiAxis_Y);
    const bool user_scroll_active
        = state != NULL && g.ActiveId == GetWindowScrollbarID(draw_window, ImGuiAxis_Y);

    bool clear_active_id = false;
    bool select_all      = false;

    int const prev_cur_pos = (state ? state->Stb.cursor : 0);

    const bool init_make_active
        = (focus_requested || user_clicked || user_scroll_finish || user_nav_input_start);
    const bool init_state = (init_make_active || user_scroll_active);
    if (init_state && g.ActiveId != id) {
        // Access state even if we don't own it yet.
        state = &g.InputTextState;
        state->CursorAnimReset();

        // Take a copy of the initial buffer value (both in original UTF-8 format and converted to
        // wchar) From the moment we focused we are ignoring the content of 'buf' (unless we are in
        // read-only mode)
        const int buf_len = (int)strlen(buf);
        state->InitialTextA.resize(buf_len + 1); // UTF-8. we use +1 to make sure that .Data is
                                                 // always pointing to at least an empty string.
        memcpy(state->InitialTextA.Data, buf, size_t(buf_len + 1));

        // Start edition
        const char* buf_end = NULL;
        state->TextW.resize(buf_size
                            + 1); // wchar count <= UTF-8 count. we use +1 to make sure that .Data
                                  // is always pointing to at least an empty string.
        state->TextA.resize(0);
        state->TextAIsValid = false; // TextA is not valid yet (we will display buf until then)
        state->CurLenW      = ImTextStrFromUtf8(state->TextW.Data, buf_size, buf, NULL, &buf_end);
        state->CurLenA
            = (int)(buf_end - buf); // We can't get the result from ImStrncpy() above because it is
                                    // not UTF-8 aware. Here we'll cut off malformed UTF-8.

        // Preserve cursor position and undo/redo stack if we come back to same widget
        // FIXME: For non-readonly widgets we might be able to require that TextAIsValid && TextA ==
        // buf ? (untested) and discard undo stack if user buffer has changed.
        const bool recycle_state = (state->ID == id);
        if (recycle_state) {
            // Recycle existing cursor/selection/undo stack but clamp position
            // Note a single mouse click will override the cursor/position immediately by calling
            // stb_textedit_click handler.
            state->CursorClamp();
        }
        else {
            state->ID      = id;
            state->ScrollX = 0.0f;
            stb_textedit_initialize_state(&state->Stb, false);
        }
        if (flags & ImGuiInputTextFlags_AlwaysInsertMode)
            state->Stb.insert_mode = 1;
    }

    if (g.ActiveId != id && init_make_active) {
        IM_ASSERT(state && state->ID == id);
        SetActiveID(id, window);
        SetFocusID(id, window);
        FocusWindow(window);

        // Declare our inputs
        IM_ASSERT(ImGuiNavInput_COUNT < 32);
        g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
        g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Up) | (1 << ImGuiDir_Down);
        g.ActiveIdUsingNavInputMask |= (1 << ImGuiNavInput_Cancel);
        g.ActiveIdUsingKeyInputMask |= ((ImU64)1 << ImGuiKey_Home) | ((ImU64)1 << ImGuiKey_End);

        g.ActiveIdUsingKeyInputMask |= ((ImU64)1 << ImGuiKey_PageUp)
            | ((ImU64)1 << ImGuiKey_PageDown); // FIXME-NAV: Page up/down actually not supported
                                               // yet by widget, but claim them ahead.
        if (flags
            & (ImGuiInputTextFlags_CallbackCompletion
               | ImGuiInputTextFlags_AllowTabInput)) // Disable keyboard tabbing out as we will use
                                                     // the \t character.
            g.ActiveIdUsingKeyInputMask |= ((ImU64)1 << ImGuiKey_Tab);
    }

    // We have an edge case if ActiveId was set through another widget (e.g. widget being swapped),
    // clear id immediately (don't wait until the end of the function)
    if (g.ActiveId == id && state == NULL)
        ClearActiveID();

    // Release focus when we click outside
    if (g.ActiveId == id && io.MouseClicked[0] && !init_state && !init_make_active) //-V560
        clear_active_id = true;

    // Lock the decision of whether we are going to take the path displaying the cursor or selection
    const bool render_cursor = (g.ActiveId == id) || (state && user_scroll_active);
    bool       render_selection
        = state && state->HasSelection() && (RENDER_SELECTION_WHEN_INACTIVE || render_cursor);
    bool value_changed = false;
    bool enter_pressed = false;

    // When read-only we always use the live data passed to the function
    // FIXME-OPT: Because our selection/cursor code currently needs the wide text we need to convert
    // it when active, which is not ideal :(
    if (is_readonly && state != NULL && (render_cursor || render_selection)) {
        const char* buf_end = NULL;
        state->TextW.resize(buf_size + 1);
        state->CurLenW
            = ImTextStrFromUtf8(state->TextW.Data, state->TextW.Size, buf, NULL, &buf_end);
        state->CurLenA = (int)(buf_end - buf);
        state->CursorClamp();
        render_selection &= state->HasSelection();
    }

    // Select the buffer to render.
    const bool buf_display_from_state = (render_cursor || render_selection || g.ActiveId == id)
        && !is_readonly && state && state->TextAIsValid;

    // Process mouse inputs and character inputs
    bool ask_format                 = false;
    int  backup_current_text_length = 0;
    if (g.ActiveId == id) {
        IM_ASSERT(state != NULL);
        backup_current_text_length = state->CurLenA;
        state->BufCapacityA        = buf_size;
        state->UserFlags           = flags;
        state->UserCallback        = callback;
        state->UserCallbackData    = callback_user_data;

        // Although we are active we don't prevent mouse from hovering other elements unless we are
        // interacting right now with the widget. Down the line we should have a cleaner
        // library-wide concept of Selected vs Active.
        g.ActiveIdAllowOverlap   = !io.MouseDown[0];
        g.WantTextInputNextFrame = 1;

        // Edit in progress
        const float mouse_x
            = (io.MousePos.x - frame_bb.Min.x - style.FramePadding.x) + state->ScrollX;
        const float mouse_y = (io.MousePos.y - draw_window->DC.CursorPos.y - style.FramePadding.y);

        const bool is_osx = io.ConfigMacOSXBehaviors;
        if (select_all || (hovered && !is_osx && io.MouseDoubleClicked[0])) {
            state->SelectAll();
            state->SelectedAllMouseLock = true;
        }
        else if (hovered && is_osx && io.MouseDoubleClicked[0]) {
            // Double-click select a word only, OS X style (by simulating keystrokes)
            state->OnKeyPressed(STB_TEXTEDIT_K_WORDLEFT);
            state->OnKeyPressed(STB_TEXTEDIT_K_WORDRIGHT | STB_TEXTEDIT_K_SHIFT);
        }
        else if (io.MouseClicked[0] && !state->SelectedAllMouseLock) {
            if (hovered) {
                stb_textedit_click(state, &state->Stb, mouse_x, mouse_y);
                state->CursorAnimReset();
            }
        }
        else if (io.MouseDown[0] && !state->SelectedAllMouseLock
                 && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)) {
            stb_textedit_drag(state, &state->Stb, mouse_x, mouse_y);
            state->CursorAnimReset();
            state->CursorFollow = true;
        }
        if (state->SelectedAllMouseLock && !io.MouseDown[0])
            state->SelectedAllMouseLock = false;

        if (ImGui::IsKeyPressed('F', false) && io.KeyAlt) {
            ask_format = true;
        }

        // It is ill-defined whether the back-end needs to send a \t character when pressing the TAB
        // keys. Win32 and GLFW naturally do it but not SDL.
        const bool ignore_char_inputs = (io.KeyCtrl && !io.KeyAlt) || (is_osx && io.KeySuper);
        if ((flags & ImGuiInputTextFlags_AllowTabInput) && IsKeyPressedMap(ImGuiKey_Tab)
            && !ignore_char_inputs && !io.KeyShift && !is_readonly)
            if (!io.InputQueueCharacters.contains('\t')) {
                unsigned int c = '\t'; // Insert TAB
                if (InputTextFilterCharacter(&c, flags, callback, callback_user_data))
                    state->OnKeyPressed((int)c);
            }

        // Process regular text input (before we check for Return because using some IME will
        // effectively send a Return?) We ignore CTRL inputs, but need to allow ALT+CTRL as some
        // keyboards (e.g. German) use AltGR (which _is_ Alt+Ctrl) to input certain characters.
        if (io.InputQueueCharacters.Size > 0) {
            if (!ignore_char_inputs && !is_readonly && !user_nav_input_start)
                for (int n = 0; n < io.InputQueueCharacters.Size; n++) {
                    // Insert character if they pass filtering
                    unsigned int c = (unsigned int)io.InputQueueCharacters[n];
                    if (c == '\t' && io.KeyShift)
                        continue;
                    if (InputTextFilterCharacter(&c, flags, callback, callback_user_data))
                        state->OnKeyPressed((int)c);
                }

            // Consume characters
            io.InputQueueCharacters.resize(0);
        }
    }

    // Process other shortcuts/key-presses
    if (g.ActiveId == id && !g.ActiveIdIsJustActivated && !clear_active_id) {
        IM_ASSERT(state != NULL);
        IM_ASSERT(
            io.KeyMods == GetMergedKeyModFlags()
            && "Mismatching io.KeyCtrl/io.KeyShift/io.KeyAlt/io.KeySuper vs io.KeyMods"); // We
                                                                                          // rarely
                                                                                          // do this
                                                                                          // check,
                                                                                          // but if
                                                                                          // anything
                                                                                          // let's
                                                                                          // do it
                                                                                          // here.

        const int  k_mask = (io.KeyShift ? STB_TEXTEDIT_K_SHIFT : 0);
        const bool is_osx = io.ConfigMacOSXBehaviors;
        const bool is_osx_shift_shortcut
            = is_osx && (io.KeyMods == (ImGuiKeyModFlags_Super | ImGuiKeyModFlags_Shift));
        const bool is_wordmove_key_down = is_osx
            ? io.KeyAlt
            : io.KeyCtrl; // OS X style: Text editing cursor movement using Alt instead of Ctrl
        const bool is_startend_key_down = is_osx && io.KeySuper && !io.KeyCtrl
            && !io.KeyAlt; // OS X style: Line/Text Start and End using Cmd+Arrows instead of
                           // Home/End
        const bool is_ctrl_key_only  = (io.KeyMods == ImGuiKeyModFlags_Ctrl);
        const bool is_shift_key_only = (io.KeyMods == ImGuiKeyModFlags_Shift);
        const bool is_shortcut_key   = g.IO.ConfigMacOSXBehaviors
            ? (io.KeyMods == ImGuiKeyModFlags_Super)
            : (io.KeyMods == ImGuiKeyModFlags_Ctrl);

        const bool is_cut = ((is_shortcut_key && IsKeyPressedMap(ImGuiKey_X))
                             || (is_shift_key_only && IsKeyPressedMap(ImGuiKey_Delete)))
            && !is_readonly && state->HasSelection();
        const bool is_copy = ((is_shortcut_key && IsKeyPressedMap(ImGuiKey_C))
                              || (is_ctrl_key_only && IsKeyPressedMap(ImGuiKey_Insert)))
            && state->HasSelection();
        const bool is_paste = ((is_shortcut_key && IsKeyPressedMap(ImGuiKey_V))
                               || (is_shift_key_only && IsKeyPressedMap(ImGuiKey_Insert)))
            && !is_readonly;
        const bool is_undo = ((is_shortcut_key && IsKeyPressedMap(ImGuiKey_Z)) && !is_readonly);
        const bool is_redo = ((is_shortcut_key && IsKeyPressedMap(ImGuiKey_Y))
                              || (is_osx_shift_shortcut && IsKeyPressedMap(ImGuiKey_Z)))
            && !is_readonly;

        if (IsKeyPressedMap(ImGuiKey_LeftArrow)) {
            state->OnKeyPressed(
                (is_startend_key_down
                     ? STB_TEXTEDIT_K_LINESTART
                     : is_wordmove_key_down ? STB_TEXTEDIT_K_WORDLEFT : STB_TEXTEDIT_K_LEFT)
                | k_mask);
        }
        else if (IsKeyPressedMap(ImGuiKey_RightArrow)) {
            state->OnKeyPressed(
                (is_startend_key_down
                     ? STB_TEXTEDIT_K_LINEEND
                     : is_wordmove_key_down ? STB_TEXTEDIT_K_WORDRIGHT : STB_TEXTEDIT_K_RIGHT)
                | k_mask);
        }
        else if (IsKeyPressedMap(ImGuiKey_UpArrow)) {
            if (io.KeyCtrl)
                SetScrollY(draw_window, ImMax(draw_window->Scroll.y - g.FontSize, 0.0f));
            else
                state->OnKeyPressed(
                    (is_startend_key_down ? STB_TEXTEDIT_K_TEXTSTART : STB_TEXTEDIT_K_UP) | k_mask);
        }
        else if (IsKeyPressedMap(ImGuiKey_DownArrow)) {
            if (io.KeyCtrl)
                SetScrollY(draw_window, ImMin(draw_window->Scroll.y + g.FontSize, GetScrollMaxY()));
            else
                state->OnKeyPressed(
                    (is_startend_key_down ? STB_TEXTEDIT_K_TEXTEND : STB_TEXTEDIT_K_DOWN) | k_mask);
        }
        else if (IsKeyPressedMap(ImGuiKey_Home)) {
            state->OnKeyPressed(io.KeyCtrl ? STB_TEXTEDIT_K_TEXTSTART | k_mask
                                           : STB_TEXTEDIT_K_LINESTART | k_mask);
        }
        else if (IsKeyPressedMap(ImGuiKey_End)) {
            state->OnKeyPressed(io.KeyCtrl ? STB_TEXTEDIT_K_TEXTEND | k_mask
                                           : STB_TEXTEDIT_K_LINEEND | k_mask);
        }
        else if (IsKeyPressedMap(ImGuiKey_Delete) && !is_readonly) {
            state->OnKeyPressed(STB_TEXTEDIT_K_DELETE | k_mask);
        }
        else if (IsKeyPressedMap(ImGuiKey_Backspace) && !is_readonly) {
            if (!state->HasSelection()) {
                if (is_wordmove_key_down)
                    state->OnKeyPressed(STB_TEXTEDIT_K_WORDLEFT | STB_TEXTEDIT_K_SHIFT);
                else if (is_osx && io.KeySuper && !io.KeyAlt && !io.KeyCtrl)
                    state->OnKeyPressed(STB_TEXTEDIT_K_LINESTART | STB_TEXTEDIT_K_SHIFT);
            }
            state->OnKeyPressed(STB_TEXTEDIT_K_BACKSPACE | k_mask);
        }
        else if (IsKeyPressedMap(ImGuiKey_Enter) || IsKeyPressedMap(ImGuiKey_KeyPadEnter)) {
            bool ctrl_enter_for_new_line = (flags & ImGuiInputTextFlags_CtrlEnterForNewLine) != 0;
            if ((ctrl_enter_for_new_line && !io.KeyCtrl)
                || (!ctrl_enter_for_new_line && io.KeyCtrl)) {
                enter_pressed = clear_active_id = true;
            }
            else if (!is_readonly) {
                ImGuiTextEditCallbackData data;
                data.EventFlag = CodeEditorFlags_GetCompletion;
                data.UserData  = callback_user_data;
                if (callback(&data)) {
                    size_t const complen   = size_t(data.BufTextLen);
                    const char*  compend   = data.Buf + complen;
                    ImWchar*     compw     = (ImWchar*)IM_ALLOC((complen + 1) * sizeof(ImWchar));
                    int          compw_len = 0;
                    for (const char* s = data.Buf; s <= compend;) {
                        unsigned int c;
                        s += ImTextCharFromUtf8(&c, s, NULL);
                        if (c == 0)
                            break;
                        if (!InputTextFilterCharacter(&c, flags, callback, callback_user_data))
                            continue;
                        compw[compw_len++] = (ImWchar)c;
                    }
                    int const del_size = data.SelectionStart + data.SelectionEnd;
                    compw[compw_len]   = 0;
                    state->Stb.cursor -= data.SelectionStart;
                    stb_textedit_delete(state, &state->Stb, state->Stb.cursor, del_size);
                    stb_textedit_paste(state, &state->Stb, compw, compw_len);
                    MemFree(compw);
                    state->CursorFollow = true;

                    data.Flags = CodeEditorFlags_ClearCompletion;
                    callback(&data);
                }
                else {
                    unsigned int c = '\n'; // Insert new line
                    if (InputTextFilterCharacter(&c, flags, callback, callback_user_data))
                        state->OnKeyPressed((int)c);
                }
            }
        }
        else if (IsKeyPressedMap(ImGuiKey_Escape)) {
            // clear_active_id = cancel_edit = true;
            ImGuiTextEditCallbackData data;
            data.EventFlag = CodeEditorFlags_ClearCompletion;
            data.UserData  = callback_user_data;
            callback(&data);
        }
        else if (is_undo || is_redo) {
            state->OnKeyPressed(is_undo ? STB_TEXTEDIT_K_UNDO : STB_TEXTEDIT_K_REDO);
            state->ClearSelection();
        }
        else if (is_shortcut_key && IsKeyPressedMap(ImGuiKey_A)) {
            state->SelectAll();
            state->CursorFollow = true;
        }
        else if (is_cut || is_copy) {
            // Cut, Copy
            if (io.SetClipboardTextFn) {
                const int ib = state->HasSelection()
                    ? ImMin(state->Stb.select_start, state->Stb.select_end)
                    : 0;
                const int ie = state->HasSelection()
                    ? ImMax(state->Stb.select_start, state->Stb.select_end)
                    : state->CurLenW;
                const int clipboard_data_len
                    = ImTextCountUtf8BytesFromStr(state->TextW.Data + ib, state->TextW.Data + ie)
                    + 1;
                char* clipboard_data = (char*)IM_ALLOC(clipboard_data_len * sizeof(char));
                ImTextStrToUtf8(clipboard_data, clipboard_data_len, state->TextW.Data + ib,
                                state->TextW.Data + ie);
                SetClipboardText(clipboard_data);
                MemFree(clipboard_data);
            }
            if (is_cut) {
                if (!state->HasSelection())
                    state->SelectAll();
                state->CursorFollow = true;
                stb_textedit_cut(state, &state->Stb);
            }
        }
        else if (is_paste) {
            io.InputQueueCharacters.resize(0);
            if (const char* clipboard = GetClipboardText()) {
                // Filter pasted buffer
                const int clipboard_len = (int)strlen(clipboard);
                ImWchar*  clipboard_filtered
                    = (ImWchar*)IM_ALLOC((clipboard_len + 1) * sizeof(ImWchar));
                int clipboard_filtered_len = 0;
                for (const char* s = clipboard; *s;) {
                    unsigned int c;
                    s += ImTextCharFromUtf8(&c, s, NULL);
                    if (c == 0)
                        break;
                    if (!InputTextFilterCharacter(&c, flags, callback, callback_user_data))
                        continue;
                    clipboard_filtered[clipboard_filtered_len++] = (ImWchar)c;
                }
                clipboard_filtered[clipboard_filtered_len] = 0;
                if (clipboard_filtered_len
                    > 0) // If everything was filtered, ignore the pasting operation
                {
                    stb_textedit_paste(state, &state->Stb, clipboard_filtered,
                                       clipboard_filtered_len);
                    state->CursorFollow = true;
                }
                MemFree(clipboard_filtered);
            }
        }

        // Update render selection flag after events have been handled, so selection highlight can
        // be displayed during the same frame.
        render_selection
            |= state->HasSelection() && (RENDER_SELECTION_WHEN_INACTIVE || render_cursor);
    }

    const ImVec4 clip_rect(
        frame_bb.Min.x, frame_bb.Min.y, frame_bb.Min.x + inner_size.x,
        frame_bb.Min.y + inner_size.y); // Not using frame_bb.Max because we have adjusted size
    ImVec2 draw_pos = draw_window->DC.CursorPos;
    ImVec2 text_size(0.0f, 0.0f);

    // Set upper limit of single-line InputTextEx() at 2 million characters strings. The current
    // pathological worst case is a long line without any carriage return, which would makes
    // ImFont::RenderText() reserve too many vertices and probably crash. Avoid it altogether. Note
    // that we only use this limit on single-line InputText(), so a pathologically large line on a
    // InputTextMultiline() would still crash.
    const int   buf_display_max_length = 2 * 1024 * 1024;
    const char* buf_display            = buf_display_from_state ? state->TextA.Data : buf; //-V595
    const char* buf_display_end = NULL; // We have specialized paths below for setting the length

    // Render text. We currently only render selection when the widget is active or while scrolling.
    // FIXME: We could remove the '&& render_cursor' to keep rendering selection when inactive.
    if (render_cursor || render_selection) {
        IM_ASSERT(state != NULL);

        buf_display_end = buf_display + state->CurLenA;

        // Render text (with cursor and selection)
        // This is going to be messy. We need to:
        // - Display the text (this alone can be more easily clipped)
        // - Handle scrolling, highlight selection, display cursor (those all requires some form of
        // 1d->2d cursor position calculation)
        // - Measure text height (for scrollbar)
        // We are attempting to do most of that in **one main pass** to minimize the computation
        // cost (non-negligible for large amount of text) + 2nd pass for selection rendering (we
        // could merge them by an extra refactoring effort)
        // FIXME: This should occur on buf_display but we'd need to maintain
        // cursor/select_start/select_end for UTF-8.
        const ImWchar* text_begin = state->TextW.Data;
        ImVec2         cursor_offset, select_start_offset;

        {
            // Find lines numbers straddling 'cursor' (slot 0) and 'select_start' (slot 1)
            // positions.
            const ImWchar* searches_input_ptr[2]      = { NULL, NULL };
            int            searches_result_line_no[2] = { -1000, -1000 };
            int            searches_remaining         = 0;
            if (render_cursor) {
                searches_input_ptr[0]      = text_begin + state->Stb.cursor;
                searches_result_line_no[0] = -1;
                searches_remaining++;
            }
            if (render_selection) {
                searches_input_ptr[1]
                    = text_begin + ImMin(state->Stb.select_start, state->Stb.select_end);
                searches_result_line_no[1] = -1;
                searches_remaining++;
            }

            // Iterate all lines to find our line numbers
            // In multi-line mode, we never exit the loop until all lines are counted, so add one
            // extra to the searches_remaining counter.
            searches_remaining += 1;
            int line_count = 0;
            // for (const ImWchar* s = text_begin; (s = (const ImWchar*)wcschr((const wchar_t*)s,
            // (wchar_t)'\n')) != NULL; s++)  // FIXME-OPT: Could use this when wchar_t are 16-bit
            for (const ImWchar* s = text_begin; *s != 0; s++)
                if (*s == '\n') {
                    line_count++;
                    if (searches_result_line_no[0] == -1 && s >= searches_input_ptr[0]) {
                        searches_result_line_no[0] = line_count;
                        if (--searches_remaining <= 0)
                            break;
                    }
                    if (searches_result_line_no[1] == -1 && s >= searches_input_ptr[1]) {
                        searches_result_line_no[1] = line_count;
                        if (--searches_remaining <= 0)
                            break;
                    }
                }
            line_count++;
            if (searches_result_line_no[0] == -1)
                searches_result_line_no[0] = line_count;
            if (searches_result_line_no[1] == -1)
                searches_result_line_no[1] = line_count;

            // Calculate 2d position by finding the beginning of the line and measuring distance
            cursor_offset.x = InputTextCalcTextSizeW(ImStrbolW(searches_input_ptr[0], text_begin),
                                                     searches_input_ptr[0])
                                  .x;
            cursor_offset.y = searches_result_line_no[0] * g.FontSize;
            if (searches_result_line_no[1] >= 0) {
                select_start_offset.x
                    = InputTextCalcTextSizeW(ImStrbolW(searches_input_ptr[1], text_begin),
                                             searches_input_ptr[1])
                          .x;
                select_start_offset.y = searches_result_line_no[1] * g.FontSize;
            }

            // Store text height (note that we haven't calculated text width at all, see GitHub
            // issues #383, #1224)
            text_size = ImVec2(inner_size.x, line_count * g.FontSize);
        }

        // Scroll
        if (render_cursor && state->CursorFollow) {
            // Horizontal scroll in chunks of quarter width
            if (!(flags & ImGuiInputTextFlags_NoHorizontalScroll)) {
                const float scroll_increment_x = inner_size.x * 0.25f;
                if (cursor_offset.x < state->ScrollX)
                    state->ScrollX = IM_FLOOR(ImMax(0.0f, cursor_offset.x - scroll_increment_x));
                else if (cursor_offset.x - inner_size.x >= state->ScrollX)
                    state->ScrollX = IM_FLOOR(cursor_offset.x - inner_size.x + scroll_increment_x);
            }
            else {
                state->ScrollX = 0.0f;
            }

            // Vertical scroll
            float scroll_y = draw_window->Scroll.y;
            if (cursor_offset.y - g.FontSize < scroll_y)
                scroll_y = ImMax(0.0f, cursor_offset.y - g.FontSize);
            else if (cursor_offset.y - inner_size.y >= scroll_y)
                scroll_y = cursor_offset.y - inner_size.y;
            draw_pos.y += (draw_window->Scroll.y
                           - scroll_y); // Manipulate cursor pos immediately avoid a frame of lag
            draw_window->Scroll.y = scroll_y;

            state->CursorFollow = false;
        }

        // Draw selection
        const ImVec2 draw_scroll = ImVec2(state->ScrollX, 0.0f);
        if (render_selection) {
            const ImWchar* text_selected_begin
                = text_begin + ImMin(state->Stb.select_start, state->Stb.select_end);
            const ImWchar* text_selected_end
                = text_begin + ImMax(state->Stb.select_start, state->Stb.select_end);

            ImU32 bg_color = GetColorU32(
                ImGuiCol_TextSelectedBg,
                render_cursor
                    ? 1.0f
                    : 0.6f); // FIXME: current code flow mandate that render_cursor is always true
                             // here, we are leaving the transparent one for tests.
            float bg_offy_up = 0.0f; // FIXME: those offsets should be part of the style?
                                     // they don't play so well with multi-line selection.
            float  bg_offy_dn = 0.0f;
            ImVec2 rect_pos   = draw_pos + select_start_offset - draw_scroll;
            for (const ImWchar* p = text_selected_begin; p < text_selected_end;) {
                if (rect_pos.y > clip_rect.w + g.FontSize)
                    break;
                if (rect_pos.y < clip_rect.y) {
                    // p = (const ImWchar*)wmemchr((const wchar_t*)p, '\n', text_selected_end - p);
                    // // FIXME-OPT: Could use this when wchar_t are 16-bit p = p ? p + 1 :
                    // text_selected_end;
                    while (p < text_selected_end)
                        if (*p++ == '\n')
                            break;
                }
                else {
                    ImVec2 rect_size = InputTextCalcTextSizeW(p, text_selected_end, &p, NULL, true);
                    if (rect_size.x <= 0.0f)
                        rect_size.x = IM_FLOOR(g.Font->GetCharAdvance((ImWchar)' ')
                                               * 0.50f); // So we can see selected empty lines
                    ImRect rect(rect_pos + ImVec2(0.0f, bg_offy_up - g.FontSize),
                                rect_pos + ImVec2(rect_size.x, bg_offy_dn));
                    rect.ClipWith(clip_rect);
                    if (rect.Overlaps(clip_rect))
                        draw_window->DrawList->AddRectFilled(rect.Min, rect.Max, bg_color);
                }
                rect_pos.x = draw_pos.x - draw_scroll.x;
                rect_pos.y += g.FontSize;
            }
        }

        render_colored_text(buf_display, buf_display_end, draw_pos - draw_scroll, colors);

        // Draw blinking cursor
        if (render_cursor) {
            state->CursorAnim += io.DeltaTime;
            bool cursor_is_visible = (!g.IO.ConfigInputTextCursorBlink)
                || (state->CursorAnim <= 0.0f) || ImFmod(state->CursorAnim, 1.20f) <= 0.80f;
            ImVec2 cursor_screen_pos = draw_pos + cursor_offset - draw_scroll;
            ImRect cursor_screen_rect(cursor_screen_pos.x, cursor_screen_pos.y - g.FontSize + 0.5f,
                                      cursor_screen_pos.x + 1.0f, cursor_screen_pos.y - 1.5f);
            if (cursor_is_visible && cursor_screen_rect.Overlaps(clip_rect))
                draw_window->DrawList->AddLine(cursor_screen_rect.Min, cursor_screen_rect.GetBL(),
                                               GetColorU32(ImGuiCol_Text));

            // Notify OS of text input position for advanced IME (-1 x offset so that Windows IME
            // can cover our cursor. Bit of an extra nicety.)
            if (!is_readonly)
                g.PlatformImePos
                    = ImVec2(cursor_screen_pos.x - 1.0f, cursor_screen_pos.y - g.FontSize);
        }

        // rendering completion data
        // todo maybe render by client in callback ?
        ImGuiTextEditCallbackData data;
        data.EventFlag = CodeEditorFlags_ListCompletion;
        data.UserData  = callback_user_data;
        if (callback(&data)) {
            ImGui::SetNextWindowPos(draw_pos + cursor_offset - draw_scroll);
            ImGui::Begin("autocomp", nullptr,
                         ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_AlwaysAutoResize
                             | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            auto otherprops = data.Buf;
            while (otherprops && *otherprops != '\n')
                otherprops++;
            ImGui::TextEx(data.Buf, otherprops);
            if (data.BufTextLen > otherprops - data.Buf + 1)
                ImGui::TextDisabled(otherprops + 1);
            ImGui::End();
        }
    }
    else {
        // Render text only (no selection, no cursor)
        text_size = ImVec2(inner_size.x,
                           InputTextCalcTextLenAndLineCount(buf_display, &buf_display_end)
                               * g.FontSize); // We don't need width

        render_colored_text(buf_display, buf_display_end, draw_pos, colors);
    }

    // Process callbacks and apply result back to user's buffer.
    if (g.ActiveId == id) {
        IM_ASSERT(state != NULL);
        const char* apply_new_text        = NULL;
        int         apply_new_text_length = 0;

        // Apply new value immediately - copy modified buffer back
        // Note that as soon as the input box is active, the in-widget value gets priority over
        // any underlying modification of the input buffer
        // FIXME-OPT: CPU waste to do this every time the widget is active, should mark dirty
        // state from the stb_textedit callbacks.
        if (!is_readonly) {
            state->TextAIsValid = true;
            state->TextA.resize(state->TextW.Size * 4 + 1);
            ImTextStrToUtf8(state->TextA.Data, state->TextA.Size, state->TextW.Data, NULL);
        }

        // User callback
        if (ask_format
            || (flags
                & (ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory
                   | ImGuiInputTextFlags_CallbackAlways))
                != 0) {
            IM_ASSERT(callback != NULL);

            // The reason we specify the usage semantic (Completion/History) is that Completion
            // needs to disable keyboard TABBING at the moment.
            ImGuiInputTextFlags event_flag = 0;
            ImGuiKey            event_key  = ImGuiKey_COUNT;
            if ((flags & ImGuiInputTextFlags_CallbackCompletion) != 0
                && IsKeyPressedMap(ImGuiKey_Tab)) {
                event_flag = ImGuiInputTextFlags_CallbackCompletion;
                event_key  = ImGuiKey_Tab;
            }
            else if ((flags & ImGuiInputTextFlags_CallbackHistory) != 0
                     && IsKeyPressedMap(ImGuiKey_UpArrow)) {
                event_flag = ImGuiInputTextFlags_CallbackHistory;
                event_key  = ImGuiKey_UpArrow;
            }
            else if ((flags & ImGuiInputTextFlags_CallbackHistory) != 0
                     && IsKeyPressedMap(ImGuiKey_DownArrow)) {
                event_flag = ImGuiInputTextFlags_CallbackHistory;
                event_key  = ImGuiKey_DownArrow;
            }
            else if (flags & ImGuiInputTextFlags_CallbackAlways)
                event_flag = ImGuiInputTextFlags_CallbackAlways;
            else if (ask_format) {
                event_flag = CodeEditorFlags_Format;
            }

            if (event_flag) {
                ImGuiInputTextCallbackData callback_data;
                memset(&callback_data, 0, sizeof(ImGuiInputTextCallbackData));
                callback_data.EventFlag = event_flag;
                callback_data.Flags     = flags;
                callback_data.UserData  = callback_user_data;

                callback_data.EventKey   = event_key;
                callback_data.Buf        = state->TextA.Data;
                callback_data.BufTextLen = state->CurLenA;
                callback_data.BufSize    = state->BufCapacityA;
                callback_data.BufDirty   = false;

                // We have to convert from wchar-positions to UTF-8-positions, which can be
                // pretty slow (an incentive to ditch the ImWchar buffer, see
                // https://github.com/nothings/stb/issues/188)
                ImWchar*  text            = state->TextW.Data;
                const int utf8_cursor_pos = callback_data.CursorPos
                    = ImTextCountUtf8BytesFromStr(text, text + state->Stb.cursor);
                const int utf8_selection_start = callback_data.SelectionStart
                    = ImTextCountUtf8BytesFromStr(text, text + state->Stb.select_start);
                const int utf8_selection_end = callback_data.SelectionEnd
                    = ImTextCountUtf8BytesFromStr(text, text + state->Stb.select_end);

                // Call user code
                callback(&callback_data);

                // Read back what user may have modified
                IM_ASSERT(callback_data.Buf == state->TextA.Data); // Invalid to modify those fields
                IM_ASSERT(callback_data.BufSize == state->BufCapacityA);
                IM_ASSERT(callback_data.Flags == flags);
                if (callback_data.CursorPos != utf8_cursor_pos) {
                    state->Stb.cursor = ImTextCountCharsFromUtf8(
                        callback_data.Buf, callback_data.Buf + callback_data.CursorPos);
                    state->CursorFollow = true;
                }
                if (callback_data.SelectionStart != utf8_selection_start) {
                    state->Stb.select_start = ImTextCountCharsFromUtf8(
                        callback_data.Buf, callback_data.Buf + callback_data.SelectionStart);
                }
                if (callback_data.SelectionEnd != utf8_selection_end) {
                    state->Stb.select_end = ImTextCountCharsFromUtf8(
                        callback_data.Buf, callback_data.Buf + callback_data.SelectionEnd);
                }
                if (callback_data.BufDirty) {
                    IM_ASSERT(callback_data.BufTextLen
                              == (int)strlen(callback_data.Buf)); // You need to maintain BufTextLen
                                                                  // if you change the text!
                    if (callback_data.BufTextLen > backup_current_text_length && is_resizable)
                        state->TextW.resize(
                            state->TextW.Size
                            + (callback_data.BufTextLen - backup_current_text_length));
                    state->CurLenW = ImTextStrFromUtf8(state->TextW.Data, state->TextW.Size,
                                                       callback_data.Buf, NULL);
                    state->CurLenA
                        = callback_data.BufTextLen; // Assume correct length and valid UTF-8
                                                    // from user, saves us an extra strlen()
                    state->CursorAnimReset();
                }
            }
        }

        // Will copy result string if modified
        if (!is_readonly && strcmp(state->TextA.Data, buf) != 0) {
            apply_new_text        = state->TextA.Data;
            apply_new_text_length = state->CurLenA;
        }

        // Copy result to user buffer
        if (apply_new_text) {
            // We cannot test for 'backup_current_text_length != apply_new_text_length' here because
            // we have no guarantee that the size of our owned buffer matches the size of the string
            // object held by the user, and by design we allow InputText() to be used without any
            // storage on user's side.
            IM_ASSERT(apply_new_text_length >= 0);
            if (is_resizable) {
                ImGuiInputTextCallbackData callback_data;
                callback_data.EventFlag  = ImGuiInputTextFlags_CallbackResize;
                callback_data.Flags      = flags;
                callback_data.Buf        = buf;
                callback_data.BufTextLen = apply_new_text_length;
                callback_data.BufSize    = ImMax(buf_size, apply_new_text_length + 1);
                callback_data.UserData   = callback_user_data;
                callback(&callback_data);
                buf                   = callback_data.Buf;
                buf_size              = callback_data.BufSize;
                apply_new_text_length = ImMin(callback_data.BufTextLen, buf_size - 1);
                IM_ASSERT(apply_new_text_length <= buf_size);
            }
            // IMGUI_DEBUG_LOG("InputText(\"%s\"): apply_new_text length %d\n", label,
            // apply_new_text_length);

            // If the underlying buffer resize was denied or not carried to the next frame,
            // apply_new_text_length+1 may be >= buf_size.
            ImStrncpy(buf, apply_new_text, (size_t)ImMin(apply_new_text_length + 1, buf_size));
            value_changed = true;
        }

        if (apply_new_text || prev_cur_pos != state->Stb.cursor) {
            // Look for autocompletion
            ImGuiTextEditCallbackData data;
            data.EventFlag  = CodeEditorFlags_UpdateCompletion;
            data.Buf        = (char*)state->TextA.Data;
            data.BufTextLen = state->CurLenA;
            data.CursorPos  = ImTextCountUtf8BytesFromStr(state->TextW.begin(),
                                                         state->TextW.begin() + state->Stb.cursor);
            data.UserData   = callback_user_data;
            if (!callback(&data)) {
                data.EventFlag = CodeEditorFlags_ClearCompletion;
                callback(&data);
            }
        }

        // Clear temporary user storage
        state->UserFlags        = 0;
        state->UserCallback     = NULL;
        state->UserCallbackData = NULL;
    }

    // Release active ID at the end of the function (so e.g. pressing Return still does a final
    // application of the value)
    if (clear_active_id && g.ActiveId == id)
        ClearActiveID();

    Dummy(text_size + ImVec2(0.0f, g.FontSize)); // Always add room to scroll an extra line
    EndChild();
    EndGroup();

    // Log as text
    if (g.LogEnabled)
        LogRenderedText(&draw_pos, buf_display, buf_display_end);

    if (value_changed && !(flags & ImGuiInputTextFlags_NoMarkEdited))
        MarkItemEdited(id);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.ItemFlags);
    if ((flags & ImGuiInputTextFlags_EnterReturnsTrue) != 0)
        return enter_pressed;
    else
        return value_changed;
}
