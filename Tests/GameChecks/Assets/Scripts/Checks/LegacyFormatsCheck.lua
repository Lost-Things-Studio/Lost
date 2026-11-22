-- Overload's files still read, as Docs/AssetFormats.md says: this scene (LegacyFormats.ovscene), and a material, a
-- prefab and an animation graph under Overload's names and without a version (Assets/Legacy), by either name; and the
-- project's and the engine's files, now under Lost's names, asked for by Overload's
local Check = require("Scripts/Checks/Check")

local LegacyFormatsCheck = {}
local oCheck = Check.New("LegacyFormats")
local nFrame = 0
local tAnimators = {}

local function Mannequin(sName, nX, sGraph)
	local oActor = Scenes.GetCurrentScene():CreateActor(sName, "")
	oActor:GetTransform():SetLocalPosition(Vector3.new(nX, 0, 0))
	oActor:AddModelRenderer():SetModel(Resources.GetModel(":Models/Mannequin.fbx"))
	oActor:AddMaterialRenderer():SetMaterial(0, Resources.GetMaterial(":Materials/Default.ovmat"))
	oActor:AddSkinnedMeshRenderer()
	local oAnimator = oActor:AddAnimator()
	oAnimator:SetGraphPath(sGraph)
	tAnimators[sGraph] = oAnimator
end

function LegacyFormatsCheck:OnStart()
	Mannequin("ByOverloadName", 0, "Legacy/Legacy.ovanim")
	Mannequin("ByLostName", 1.2, "Legacy/Legacy.loanim")
	Mannequin("ConvertedByOverloadName", 2.4, "Animations/Mannequin.ovanim")
end

function LegacyFormatsCheck:OnUpdate()
	nFrame = nFrame + 1
	if nFrame ~= 3 then
		return
	end

	-- A material under Overload's name, without a version, its shader named by Overload's name of the engine's
	local oLegacy = Resources.GetMaterial("Legacy/Legacy.ovmat")
	oCheck:Expect(oLegacy ~= nil, "a material under Overload's name, without a version, loads")
	oCheck:Expect(Resources.GetMaterial("Legacy/Legacy.lomat") == oLegacy, "Lost's name gives the same material")

	-- Files now under Lost's names, asked for by Overload's
	local oGround = Resources.GetMaterial("Materials/Ground.lomat")
	oCheck:Expect(oGround ~= nil, "a converted material loads")
	oCheck:Expect(Resources.GetMaterial("Materials/Ground.ovmat") == oGround, "Overload's name gives the converted material")
	local oStandard = Resources.GetShader(":Shaders/Standard.lofx")
	oCheck:Expect(oStandard ~= nil, "the engine's shader loads under Lost's name")
	oCheck:Expect(Resources.GetShader(":Shaders/Standard.ovfx") == oStandard, "Overload's name gives the engine's shader")
	oCheck:Expect(Resources.GetMaterial(":Materials/Default.ovmat") == Resources.GetMaterial(":Materials/Default.lomat"), "and the engine's material")

	-- A prefab under Overload's name, without a version, by either name
	for _, sPath in ipairs({ "Legacy/Legacy.ovprefab", "Legacy/Legacy.loprefab", "Prefabs/Pair.ovprefab" }) do
		local oPrefab = Prefab()
		oPrefab.path = sPath
		local oInstance = Scenes.GetCurrentScene():InstantiatePrefab(oPrefab)
		oCheck:Expect(oInstance ~= nil and oInstance:FindChild("Target", false) ~= nil, sPath .. " is instantiated, its child with it")
	end

	-- An animation graph under Overload's name, without a version, by either name; a converted one by Overload's
	for sGraph, oAnimator in pairs(tAnimators) do
		oCheck:Expect(oAnimator:IsLoaded(), sGraph .. " plays: " .. tostring(oAnimator:GetError()))
		oCheck:Equal(oAnimator:GetState(), "Locomotion", sGraph .. " starts in its first state")
	end

	oCheck:Finish()
end

return LegacyFormatsCheck
