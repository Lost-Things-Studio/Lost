---@meta

--- Declares a list property of a Behaviour: the inspector edits its items as one comma-separated line (a string item
--- holds no comma), and the script receives a plain table. Raises an error for another item type, or an item not of it
---@param itemType "number"|"string"|"bool"
---@param items? (number|string|boolean)[] empty when nil
---@return table
function List(itemType, items) end

--- What List makes in a Behaviour's table, until the engine reads it: the script then finds a plain table in its place
---@class ListProperty
ListProperty = {}
