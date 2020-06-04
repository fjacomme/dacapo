// todo
// double click select word
// insert autocomplete bug

#include "ui/codeedit.hpp"

#include "app.hpp"
#include "internal/imgui_codeeditor.h"

#include <algorithm>
#include <array>
#include <set>

bool contains_all_of(std::string const& s, std::string const& part)
{
    size_t ip = 0;
    size_t is = 0;
    for (; is < s.size() && ip < part.size(); is++) {
        if (s[is] == part[ip]) {
            ip++;
        }
    }
    return (ip == part.size());
}

struct codeedit::pimpl {
    app&              ap;
    std::string const mixname;
    std::string       filename;

    app::mix& mx;

    std::array<ImU32, size_t(char_type::_count)> const palette = {
        0xffffffff, // none
        0xffffaaaa, // num
        0xffffaaff, // kw
        0xffaaffaa, // var
        0xffffffaa, // str
        0xffaaffff, // op
        0xffaaffff, // brack
        0xffaaaaaa, // comment
        0xff0000ff, // error
    };
    std::vector<ImU32> colors;

    pimpl(app& ap, std::string const& mn, app::mix& m)
        : ap(ap)
        , mixname(mn)
        , mx(m)
    {
        update_colors();
    }

    void parse()
    {
        ap.parse(mx);
        update_colors();
    }
    void update_colors()
    {
        auto const& ct = mx.pars.char_types;
        colors.resize(ct.size());
        std::transform(ct.begin(), ct.end(), colors.begin(),
                       [this](char_type t) { return palette[size_t(t)]; });
    }

    static int InputTextCallback(ImGuiInputTextCallbackData* data)
    {
        pimpl* self = (pimpl*)data->UserData;
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            self->resize_buffer(data);
            return 0;
        }
        if (data->EventFlag == CodeEditorFlags_ClearCompletion)
            return self->clear_completion(data);
        if (data->EventFlag == CodeEditorFlags_GetCompletion)
            return self->get_completion(data);
        if (data->EventFlag == CodeEditorFlags_ListCompletion)
            return self->list_completion(data);
        if (data->EventFlag == CodeEditorFlags_UpdateCompletion)
            return self->update_completion(data);
        if (data->EventFlag == CodeEditorFlags_Format)
            return self->format_buffer(data);
        return 0;
    }
    void draw(ImVec2 const& size)
    {
        ImGui::BeginChild(filename.c_str(), size, true);
        if (filename != mx.pars.filename) {
            filename = mx.pars.filename;
            update_colors();
        }
        auto const& buffer = mx.pars.buffer;
        ImGui::Text("%s %s", filename.c_str(), mx.saved ? "" : "*");
        if (CodeEditor(filename.c_str(), (char*)buffer.c_str(), (int)buffer.capacity() + 1,
                       colors.empty() ? nullptr : colors.data(), ImVec2(-FLT_MIN, -1), 0,
                       InputTextCallback, this)) {
            mx.saved = false;
            parse();
        }
        if (!mx.pars.error.empty()) {
            ImGui::BeginTooltip();
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "%d:%d %s", mx.pars.line, mx.pars.col,
                               mx.pars.error.c_str());
            ImGui::EndTooltip();
        }
        ImGui::EndChild();
    }

    private:
    std::string completionsdata;
    std::string completion;
    int         completionoffset = 0;
    int         completiononset  = 0;
    void        resize_buffer(ImGuiInputTextCallbackData* data)
    {
        mx.pars.buffer.resize(size_t(data->BufTextLen));
        data->Buf     = (char*)mx.pars.buffer.c_str();
        data->BufSize = (int)mx.pars.buffer.capacity();
    }
    bool format_buffer(ImGuiInputTextCallbackData* data)
    {
        std::string formated;
        formated.reserve(mx.pars.buffer.size());
        ::print(mx.pars.tree, formated);
        if (formated == mx.pars.buffer) {
            return false;
        }
        mx.pars.buffer.swap(formated);
        strcpy(data->Buf, mx.pars.buffer.c_str());
        data->BufTextLen = (int)mx.pars.buffer.size();
        data->BufDirty   = true;
        return true;
    }
    bool list_completion(ImGuiInputTextCallbackData* data)
    {
        data->Buf            = (char*)completionsdata.c_str();
        data->BufTextLen     = int(completionsdata.size());
        data->SelectionStart = 0;
        data->SelectionEnd   = 0;
        return completionsdata.size() > 0;
    }
    bool get_completion(ImGuiInputTextCallbackData* data)
    {
        data->Buf            = (char*)completion.c_str();
        data->BufTextLen     = (int)completion.size();
        data->SelectionStart = completionoffset;
        data->SelectionEnd   = completiononset;
        return completion.size() > 0;
    }
    bool clear_completion(ImGuiInputTextCallbackData*)
    {
        completionsdata.clear();
        completion.clear();
        completionoffset = 0;
        completiononset  = 0;
        return true;
    }
    bool update_completion(ImGuiInputTextCallbackData* data)
    {
        std::vector<std::string> completions;

        auto databgn = data->Buf;
        auto datacur = data->Buf + data->CursorPos;
        auto dataend = data->Buf + data->BufTextLen;

        auto tokb = datacur;
        if (tokb == databgn) {
            return false;
        }
        int nbquote = 0;
        tokb--;
        while (tokb != databgn && *tokb != '\n') {
            if (*tokb == '\'')
                nbquote++;
            tokb--;
        }
        if ((nbquote - 1) % 2) {
            return false;
        }

        tokb = datacur - 1;
        if (*tokb == '\'') {
            for (auto& s : ap.sg.defs.samples) {
                completions.push_back(s);
            }
            for (auto& s : ap.sg.defs.synths) {
                completions.push_back(s);
            }
            std::sort(completions.begin(), completions.end());
        }
        else {
            while (*tokb != '\'') {
                if (*tokb == '\n')
                    return false;
                if (*tokb == ' ')
                    return false;
                tokb--;
                if (tokb == databgn)
                    return false;
            }
            std::string const part(tokb + 1, datacur);

            for (auto& s : ap.sg.defs.samples) {
                if (contains_all_of(s, part))
                    completions.push_back(s);
            }
            for (auto& s : ap.sg.defs.synths) {
                if (contains_all_of(s, part))
                    completions.push_back(s);
            }
        }
        completion.clear();
        if (!completions.empty()) {
            completion       = *completions.begin() + '\'';
            completionoffset = int(datacur - tokb - 1);
            auto toke        = datacur;
            completiononset  = 0;
            while (toke != dataend && *toke != ' ' && *toke != '\n') {
                completiononset++;
                toke++;
            }
        }
        completionsdata.clear();
        for (auto& c : completions) {
            completionsdata += c + "\n";
        }
        data->Buf        = (char*)completionsdata.c_str();
        data->BufTextLen = (int)completionsdata.size();
        return completions.size() > 0;
    }

    pimpl(pimpl const&) = delete;
    pimpl& operator=(pimpl const&) = delete;
};

codeedit::codeedit(app& a)
    : ap(a)
{
    reset_editors();
}

codeedit::~codeedit()
{
}

void codeedit::draw()
{
    assert(!editors.empty());
    if (editors.size() != ap.mixes.size()
        || editors[0]->filename != ap.mixes.begin()->second.pars.filename) {
        reset_editors();
    }

    auto const   colsdiv = div((int)editors.size(), 2);
    auto const   cols1   = (size_t)ceil(editors.size() / 2.0);
    ImVec2 const canvas_size(ImGui::GetWindowSize().x - 20, ImGui::GetWindowSize().y - 20);

    float const h = editors.size() > 2 ? canvas_size.y / 2 : canvas_size.y;
    {
        float const w = canvas_size.x / cols1;

        for (size_t i = 0; i < cols1; i++) {
            ImGui::SameLine();
            editors[i]->draw(ImVec2(w, h));
        }
    }
    if (editors.size() > 2) {
        ImGui::NewLine();
        auto const  cols2 = editors.size() - cols1;
        float const w     = canvas_size.x / cols2;

        for (size_t i = size_t(cols1); i < editors.size(); i++) {
            ImGui::SameLine();
            editors[i]->draw(ImVec2(w, h));
        }
    }
}

void codeedit::reset_editors()
{
    editors.clear();
    for (auto& mix : ap.mixes) {
        editors.emplace_back(std::make_unique<pimpl>(ap, mix.first, mix.second));
    }
}