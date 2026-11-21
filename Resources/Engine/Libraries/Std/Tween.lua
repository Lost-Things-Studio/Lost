--- Values carried from one to another over time, on game time or on real time. They advance by themselves once a
--- frame, after every Behaviour's OnLateUpdate.
---
--- ```lua
--- local Std = require(":Libraries/Std/Init")
---
--- Std.Tween.Run({
---     duration = 0.4, from = Vector3.new(0, 0, 0), to = Vector3.new(0, 2, 0), ease = Std.Ease.OutBack,
---     onUpdate = function(position) transform:SetLocalPosition(position) end,
---     owner = self
--- }):Then({ duration = 0.2, onUpdate = function(alpha) image:SetTint(Vector4.new(1, 1, 1, 1 - alpha)) end })
--- ```
---
--- The spec of Run and Then:
--- - `duration`, `delay`: seconds. The start is applied by Run itself; a tween with neither ends at the next frame;
--- - `from`, `to`: 0 and 1 by default, or anything that adds and scales, such as a Vector3;
--- - `ease`: a curve of Std.Ease, or any function of [0, 1] (linear by default);
--- - `onUpdate(value, progress)`: called with the value, and the eased share of the run;
--- - `onDone()`: once the tween ends, never after Cancel;
--- - `loops`: runs (1 by default, 0 forever); `yoyo`: every other run backwards;
--- - `unscaled`: real time; `owner`: an actor, or a Behaviour's table, the tween ending with that actor.
---
--- A function of the spec that raises an error is logged and its tween stopped.
---@class StdTweenModule
local Tween = {}

local Owner = require(":Libraries/Std/Owner")

-- Runs: a tween this close to its end is over (steps added up drift by a rounding)
local kEpsilon = 1e-6

local running = {}
local added = {}
local advancing = nil -- the tweens Advance walks, while it runs, so that Clear reaches them too
local driven = false
local Advance

---@class StdTween
local Instance = {}
Instance.__index = Instance

local function Fail(message)
	Debug.LogError("Std.Tween: a function failed and its tween is stopped: " .. tostring(message))
end

--- Calls onUpdate with the value at a share of a run; false if it failed. The curve and the value are worked out
--- inside the protected call too: a curve that raises, or values that do not add up, stop this tween only
local function Apply(tween, share)
	tween.m_progress = share

	local ok, message = pcall(function()
		local eased = tween.m_ease ~= nil and tween.m_ease(share) or share

		if tween.m_onUpdate ~= nil then
			tween.m_onUpdate(tween.m_from + (tween.m_to - tween.m_from) * eased, eased)
		end
	end)

	if not ok then
		Fail(message)
	end

	return ok
end

local function Drive()
	if driven then
		return
	end

	driven = true

	Events.AddLateUpdate(function(deltaTime)
		local ok, message = pcall(Advance, deltaTime, Time.GetUnscaledDeltaTime())

		if not ok then
			Debug.LogError("Std.Tween: " .. tostring(message))
		end
	end)
end

local function Start(tween)
	tween.m_running = true

	if not Apply(tween, 0) then
		tween.m_running = false
		return tween
	end

	added[#added + 1] = tween
	Drive()
	return tween
end

--- A tween made from its spec, not started
local function New(spec, functionName)
	if type(spec) ~= "table" then
		error("Std.Tween: " .. functionName .. " takes a table", 3)
	end

	for _, key in ipairs({ "ease", "onUpdate", "onDone" }) do
		if spec[key] ~= nil and type(spec[key]) ~= "function" then
			error("Std.Tween: '" .. key .. "' is a function", 3)
		end
	end

	Owner.Check(spec.owner, "Std.Tween", 3)

	local function Seconds(value)
		return math.max(type(value) == "number" and value or 0, 0)
	end

	return setmetatable({
		m_duration = Seconds(spec.duration),
		m_delay = Seconds(spec.delay),
		m_from = spec.from == nil and 0 or spec.from,
		m_to = spec.to == nil and 1 or spec.to,
		m_ease = spec.ease,
		m_onUpdate = spec.onUpdate,
		m_onDone = spec.onDone,
		m_loops = math.max(math.floor(type(spec.loops) == "number" and spec.loops or 1), 0),
		m_yoyo = spec.yoyo == true,
		m_unscaled = spec.unscaled == true,
		m_owner = spec.owner,
		m_elapsed = 0,
		m_progress = 0,
		m_running = false
	}, Instance)
end

--- Starts a tween
---@param spec table
---@return StdTween
function Tween.Run(spec)
	return Start(New(spec, "Run"))
end

--- The share the last run ends on: backwards after an even number of yoyo runs
local function EndShare(tween)
	return (tween.m_yoyo and tween.m_loops > 0 and tween.m_loops % 2 == 0) and 0 or 1
end

--- Returns true until the tween ends or is cancelled
---@return boolean
function Instance:IsRunning()
	return self.m_running
end

--- Returns the share of the current run, before its curve
---@return number
function Instance:GetProgress()
	return self.m_progress
end

--- Chains a tween, started when this one ends. Returns it
---@param spec table
---@return StdTween
function Instance:Then(spec)
	local nextTween = New(spec, "Then")
	nextTween.m_held = true
	self.m_next = nextTween
	return nextTween
end

--- Stops the tween where it is, without onDone, and the tweens chained after it
function Instance:Cancel()
	self.m_running = false
	self.m_held = false

	if self.m_next ~= nil then
		self.m_next:Cancel()
	end
end

--- Ends the tween at once: its end applied, then onDone, then the chained tween starts
function Instance:Finish()
	if not self.m_running then
		return
	end

	self.m_running = false

	if not Apply(self, EndShare(self)) then
		return
	end

	if self.m_onDone ~= nil then
		local ok, message = pcall(self.m_onDone)

		if not ok then
			return Fail(message)
		end
	end

	if self.m_next ~= nil and self.m_next.m_held then
		self.m_next.m_held = false
		Start(self.m_next)
	end
end

--- One step of a tween; returns whether it goes on
local function Step(tween, gameStep, realStep)
	if not tween.m_running then
		return false
	end

	if Owner.IsGone(tween.m_owner) then
		tween.m_running = false
		return false
	end

	tween.m_elapsed = tween.m_elapsed + math.max((tween.m_unscaled and realStep or gameStep) or 0, 0)

	local time = tween.m_elapsed - tween.m_delay

	if time < 0 then
		return true
	end

	local runs = tween.m_duration > 0 and time / tween.m_duration or math.huge

	if runs == math.huge or (tween.m_loops > 0 and runs >= tween.m_loops - kEpsilon) then
		tween:Finish()
		return false
	end

	local run = math.floor(runs)
	local share = runs - run

	if tween.m_yoyo and run % 2 == 1 then
		share = 1 - share
	end

	if not Apply(tween, share) then
		tween.m_running = false
		return false
	end

	return true
end

--- Advances every tween, once a frame
Advance = function(gameStep, realStep)
	local list = running

	for i = 1, #added do
		list[#list + 1] = added[i]
	end

	-- A function may start tweens (to added) or clear them all: the list walked stays the same meanwhile
	added = {}
	running = {}
	advancing = list

	for i = 1, #list do
		local tween = list[i]

		if Step(tween, gameStep, realStep) then
			running[#running + 1] = tween
		end
	end

	advancing = nil
end

--- Stops every tween, without onDone
function Tween.Clear()
	for _, list in ipairs({ running, added, advancing or {} }) do
		for i = 1, #list do
			list[i]:Cancel()
		end
	end

	running = {}
	added = {}
end

return Tween
