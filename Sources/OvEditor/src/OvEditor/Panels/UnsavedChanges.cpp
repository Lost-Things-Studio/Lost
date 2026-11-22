/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <imgui.h>

#include <OvEditor/Core/EditorActions.h>
#include <OvEditor/Panels/UnsavedChanges.h>

namespace
{
	constexpr const char* kPopupID = "Unsaved Changes###UnsavedChangesPrompt";
}

void OvEditor::Panels::UnsavedChanges::Ask(const std::string& p_sceneName, const std::string& p_what, std::function<void(EChoice)> p_answer)
{
	m_sceneName = p_sceneName;
	m_what = p_what;
	m_answer = std::move(p_answer);
	m_asking = true;
	m_toOpen = true;
}

bool OvEditor::Panels::UnsavedChanges::IsAsking() const
{
	return m_asking;
}

void OvEditor::Panels::UnsavedChanges::_Draw_Impl()
{
	if (!m_asking)
	{
		return;
	}

	if (m_toOpen)
	{
		ImGui::OpenPopup(kPopupID);
		m_toOpen = false;
	}

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, { 0.5f, 0.5f });

	if (ImGui::BeginPopupModal(kPopupID, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::Text("The scene \"%s\" has unsaved changes.", m_sceneName.c_str());
		ImGui::Text("Save them before %s?", m_what.c_str());
		ImGui::Spacing();

		const float width = ImGui::CalcTextSize("Don't Save").x + ImGui::GetStyle().FramePadding.x * 4.0f;

		if (ImGui::Button("Save", { width, 0.0f }) || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
		{
			ImGui::CloseCurrentPopup();
			Answer(EChoice::SAVE);
		}

		ImGui::SameLine();

		if (ImGui::Button("Don't Save", { width, 0.0f }))
		{
			ImGui::CloseCurrentPopup();
			Answer(EChoice::DONT_SAVE);
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel", { width, 0.0f }) || ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			ImGui::CloseCurrentPopup();
			Answer(EChoice::CANCEL);
		}

		ImGui::EndPopup();
	}
	else if (!m_toOpen)
	{
		// Closed another way (the window lost): staying is the safe answer
		Answer(EChoice::CANCEL);
	}
}

void OvEditor::Panels::UnsavedChanges::Answer(EChoice p_choice)
{
	if (!m_asking)
	{
		return;
	}

	m_asking = false;

	// The action leaves the scene, which the panels being drawn still show: it runs once the frame is over
	EDITOR_EXEC(DelayAction([answer = std::move(m_answer), p_choice] { answer(p_choice); }, 0));
	m_answer = nullptr;
}
