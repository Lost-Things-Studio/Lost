/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <OvTools/Eventing/Event.h>

namespace OvTools::Utils
{
	/**
	* A stack of edits, to undo and redo them. An edit keeps the state before it and the state after it, as text, and
	* what they are the state of (its target). Putting a state back is up to the user of the history: it reads the
	* entry, writes its state back, then moves the history's cursor
	*/
	template<typename Target>
	class StateHistory
	{
	public:
		struct Entry
		{
			std::string label;
			Target target{};
			std::string before;
			std::string after;
		};

		/**
		* Adds an edit, after what is done. What could have been redone is dropped. Nothing is added if the two states
		* are the same
		* @param p_entry
		* @return true if the edit was added
		*/
		bool Record(Entry p_entry)
		{
			if (p_entry.before == p_entry.after)
			{
				return false;
			}

			// What could have been redone goes, and with it the saved state if it was there
			while (m_entries.size() > m_cursor)
			{
				m_size -= SizeOf(m_entries.back());
				m_entries.pop_back();
			}

			if (m_saved && *m_saved > m_cursor)
			{
				m_saved.reset();
			}

			m_size += SizeOf(p_entry);
			m_entries.push_back(std::move(p_entry));
			++m_cursor;
			Trim();
			ChangedEvent.Invoke();
			return true;
		}

		/**
		* The edit Undo would undo, or nullptr
		*/
		const Entry* GetUndoEntry() const
		{
			return CanUndo() ? &m_entries[m_cursor - 1] : nullptr;
		}

		/**
		* The edit Redo would redo, or nullptr
		*/
		const Entry* GetRedoEntry() const
		{
			return CanRedo() ? &m_entries[m_cursor] : nullptr;
		}

		/**
		* Moves the cursor before the last edit, once its state before is put back
		*/
		void StepBack()
		{
			if (CanUndo())
			{
				--m_cursor;
				ChangedEvent.Invoke();
			}
		}

		/**
		* Moves the cursor after the last undone edit, once its state after is put back
		*/
		void StepForward()
		{
			if (CanRedo())
			{
				++m_cursor;
				ChangedEvent.Invoke();
			}
		}

		bool CanUndo() const { return m_cursor > 0; }
		bool CanRedo() const { return m_cursor < m_entries.size(); }

		/**
		* The label of the edit Undo would undo, or an empty string
		*/
		const std::string& GetUndoLabel() const { return CanUndo() ? m_entries[m_cursor - 1].label : Nothing(); }

		/**
		* The label of the edit Redo would redo, or an empty string
		*/
		const std::string& GetRedoLabel() const { return CanRedo() ? m_entries[m_cursor].label : Nothing(); }

		/**
		* Forgets every edit. What is edited counts as saved, unless p_keepDirty and it was not
		* @param p_keepDirty
		*/
		void Clear(bool p_keepDirty = false)
		{
			const bool dirty = IsDirty();
			m_entries.clear();
			m_cursor = 0;
			m_size = 0;
			m_saved = p_keepDirty && dirty ? std::nullopt : std::optional<size_t>(0);
			ChangedEvent.Invoke();
		}

		/**
		* What is edited, as it is now, is the state saved
		*/
		void MarkSaved()
		{
			m_saved = m_cursor;
			ChangedEvent.Invoke();
		}

		/**
		* True if edits were made, or undone, since what is edited was saved
		*/
		bool IsDirty() const { return !m_saved || *m_saved != m_cursor; }

		size_t GetEntryCount() const { return m_entries.size(); }

		/**
		* The memory the entries' states take, in bytes
		*/
		size_t GetSize() const { return m_size; }

		/**
		* Bounds what is kept: the oldest edits go once either limit is passed (the furthest from being redone, if all are
		* undone). One edit always stays. 200 edits and 128 MB by default
		* @param p_maxEntries
		* @param p_maxBytes
		*/
		void SetLimits(size_t p_maxEntries, size_t p_maxBytes)
		{
			m_maxEntries = p_maxEntries > 0 ? p_maxEntries : 1; // Not std::max: windows.h may define max
			m_maxBytes = p_maxBytes;
			Trim();
			ChangedEvent.Invoke();
		}

		/**
		* After any change of the entries, the cursor or the saved mark
		*/
		OvTools::Eventing::Event<> ChangedEvent;

	private:
		static const std::string& Nothing()
		{
			static const std::string nothing;
			return nothing;
		}

		static size_t SizeOf(const Entry& p_entry)
		{
			return p_entry.label.size() + p_entry.before.size() + p_entry.after.size();
		}

		void Trim()
		{
			while (m_entries.size() > 1 && (m_entries.size() > m_maxEntries || m_size > m_maxBytes))
			{
				if (m_cursor > 0)
				{
					// The oldest edit done goes: the state before it can no longer come back
					m_size -= SizeOf(m_entries.front());
					m_entries.erase(m_entries.begin());
					--m_cursor;

					if (m_saved)
					{
						m_saved = *m_saved == 0 ? std::nullopt : std::optional<size_t>(*m_saved - 1);
					}
				}
				else
				{
					// Everything is undone: the edit furthest from being redone goes
					m_size -= SizeOf(m_entries.back());
					m_entries.pop_back();

					if (m_saved && *m_saved > m_entries.size())
					{
						m_saved.reset();
					}
				}
			}
		}

		std::vector<Entry> m_entries;
		size_t m_cursor = 0; // The entries before it are done, the ones from it on undone
		std::optional<size_t> m_saved = 0; // Where the cursor was when what is edited was saved, if that state can come back
		size_t m_size = 0;
		size_t m_maxEntries = 200;
		size_t m_maxBytes = 128 * 1024 * 1024;
	};
}
