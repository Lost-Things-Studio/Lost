/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <functional>
#include <string>

#include <OvUI/Panels/APanel.h>

namespace OvEditor::Panels
{
	/**
	* Asks, in a modal window, what to do with the scene's unsaved changes before an action leaves the scene: save them,
	* drop them, or stay
	*/
	class UnsavedChanges : public OvUI::Panels::APanel
	{
	public:
		enum class EChoice { SAVE, DONT_SAVE, CANCEL };

		/**
		* Opens the window. The answer is given once the frame is over, never while the window is drawn
		* @param p_sceneName
		* @param p_what (what the action does, as "opening another scene")
		* @param p_answer
		*/
		void Ask(const std::string& p_sceneName, const std::string& p_what, std::function<void(EChoice)> p_answer);

		/**
		* True while the window waits for an answer
		*/
		bool IsAsking() const;

	protected:
		void _Draw_Impl() override;

	private:
		void Answer(EChoice p_choice);

		std::string m_sceneName;
		std::string m_what;
		std::function<void(EChoice)> m_answer;
		bool m_asking = false;
		bool m_toOpen = false;
	};
}
