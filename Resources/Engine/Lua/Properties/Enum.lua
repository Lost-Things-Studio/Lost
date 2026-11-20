---@meta

--- Declares an enumeration property of a Behaviour: the inspector lists the choices, and the script receives the chosen
--- one. Raises an error when there is no choice, or when the default is not one of them
---@param choices string[]
---@param default? string the first choice when nil
---@return string
function Enum(choices, default) end

--- What Enum makes in a Behaviour's table, until the engine reads it: the script then finds the chosen string in its
--- place
---@class EnumProperty
EnumProperty = {}
