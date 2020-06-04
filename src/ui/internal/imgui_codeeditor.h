#pragma once

#include "imgui.h"

enum CodeEditorFlags {
    CodeEditorFlags_Format = 1 << 22, // defined after last of ImGuiInputTextFlags
    CodeEditorFlags_ListCompletion,
    CodeEditorFlags_UpdateCompletion,
    CodeEditorFlags_GetCompletion,
    CodeEditorFlags_ClearCompletion
};

// copy-pasted from ImGui::InputTextMultiline, very messy
bool CodeEditor(const char*            name,
                char*                  buf,
                int                    buf_size,
                const ImU32*           colors,//for each char in buf, the color to use
                const ImVec2&          size_arg,
                ImGuiInputTextFlags    flags,
                ImGuiInputTextCallback callback,
                void*                  user_data);