---@meta

--- Declares a colour property of a Behaviour: the inspector shows a colour picker, and the script receives a Vector4
--- (r, g, b, a, each from 0 to 1)
---@param r? number 1 when nil
---@param g? number 1 when nil
---@param b? number 1 when nil
---@param a? number 1 when nil
---@return Vector4
function Color(r, g, b, a) end

--- What Color makes in a Behaviour's table, until the engine reads it: the script then finds a Vector4 in its place
---@class ColorProperty
ColorProperty = {}
