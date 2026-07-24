--!strict
--[[
	BOX-ONLY body export (no accessories — Roblox blocks forking catalog meshes).

	For FULL avatar WITH accessories + textures:
	  1) _G.JewsploitExportR6()  — spawns your R6 in Workspace
	  2) Select that model in Explorer
	  3) File → Export Selection… → jewsploit.obj (+ mtl/png)
	  4) Drop into Cheat/Core/Features/Visuals/preview_model/ as model.obj + texture.png

	This script alone only dumps body boxes → JewsploitModelOBJ StringValue.
]]

local HttpService = game:GetService("HttpService")
local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local Selection = game:GetService("Selection")
local ServerStorage = game:GetService("ServerStorage")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

-- Core R6 only (no accessories / Handle)
local BODY = {
	"Head",
	"Torso",
	"Left Arm",
	"Right Arm",
	"Left Leg",
	"Right Leg",
}

-- ESP skeleton order (13) — synthesized from 6 R6 parts
local JOINT_ORDER = {
	"Head",
	"UpperTorso",
	"LeftUpperArm",
	"RightUpperArm",
	"LeftLowerArm",
	"RightLowerArm",
	"LeftHand",
	"RightHand",
	"LowerTorso",
	"LeftUpperLeg",
	"RightUpperLeg",
	"LeftFoot",
	"RightFoot",
}

local function fmt(n: number): string
	return string.format("%.6f", n)
end

local function destParent(): Instance
	local probe = Instance.new("Folder")
	local ok = pcall(function()
		probe.Parent = ServerStorage
	end)
	if ok and probe.Parent == ServerStorage then
		probe:Destroy()
		return ServerStorage
	end
	pcall(function() probe:Destroy() end)
	return ReplicatedStorage
end

local function loadR6(userId: number?): Model
	local id = userId
	if not id or id <= 0 then
		if Players.LocalPlayer and Players.LocalPlayer.UserId > 0 then
			id = Players.LocalPlayer.UserId
		else
			error("[jewsploit] pass userId: _G.JewsploitExportR6(USER_ID)")
		end
	end

	print(string.format("[jewsploit] loading %d as R6…", id))
	local desc = Players:GetHumanoidDescriptionFromUserIdAsync(id)
	local model = Players:CreateHumanoidModelFromDescription(desc, Enum.HumanoidRigType.R6)
	model.Name = "JewsploitR6_" .. tostring(id)
	model.Parent = workspace
	local hrp = model:FindFirstChild("HumanoidRootPart") :: BasePart?
	if hrp then
		hrp.Anchored = true
		hrp.CFrame = CFrame.new(0, 8, 0)
	end
	return model
end

type Obj = {
	lines: { string },
	v: number,
	addV: (self: Obj, x: number, y: number, z: number) -> number,
	addVt: (self: Obj, u: number, v: number) -> number,
	addTri: (self: Obj, a: number, b: number, c: number, ta: number, tb: number, tc: number) -> (),
}

local function newObj(): Obj
	local o: Obj = {
		lines = { "# jewsploit r6 body", "o Character" },
		v = 0,
		addV = function(self, x, y, z)
			self.v += 1
			table.insert(self.lines, string.format("v %s %s %s", fmt(x), fmt(y), fmt(z)))
			return self.v
		end,
		addVt = function(self, u, v)
			table.insert(self.lines, string.format("vt %s %s", fmt(u), fmt(v)))
			return self.v -- vt index == current v count when we add 1:1; we track separately below
		end,
		addTri = function(self, a, b, c, ta, tb, tc)
			table.insert(self.lines, string.format("f %d/%d %d/%d %d/%d", a, ta, b, tb, c, tc))
		end,
	}
	return o
end

local function addBox(obj: Obj, rootCF: CFrame, part: BasePart, vtBase: { number })
	local rel = rootCF:ToObjectSpace(part.CFrame)
	local hx, hy, hz = part.Size.X * 0.5, part.Size.Y * 0.5, part.Size.Z * 0.5
	local corners = {
		Vector3.new(-hx, -hy, -hz), Vector3.new(hx, -hy, -hz),
		Vector3.new(hx, hy, -hz), Vector3.new(-hx, hy, -hz),
		Vector3.new(-hx, -hy, hz), Vector3.new(hx, -hy, hz),
		Vector3.new(hx, hy, hz), Vector3.new(-hx, hy, hz),
	}
	local p: { Vector3 } = table.create(8)
	for i = 1, 8 do
		p[i] = rel:PointToWorldSpace(corners[i])
	end

	-- 6 faces as quads → 2 tris; unique verts+uvs per face corner (clean UVs)
	local faces = {
		{ 1, 2, 3, 4 },
		{ 6, 5, 8, 7 },
		{ 5, 6, 2, 1 },
		{ 2, 6, 7, 3 },
		{ 4, 3, 7, 8 },
		{ 5, 1, 4, 8 },
	}
	local uvs = {
		Vector2.new(0, 0), Vector2.new(1, 0), Vector2.new(1, 1), Vector2.new(0, 1),
	}

	for _, face in faces do
		local idx = table.create(4)
		local tidx = table.create(4)
		for q = 1, 4 do
			local w = p[face[q]]
			local vi = obj:addV(w.X, w.Y, w.Z)
			table.insert(obj.lines, string.format("vt %s %s", fmt(uvs[q].X), fmt(uvs[q].Y)))
			vtBase[1] += 1
			idx[q] = vi
			tidx[q] = vtBase[1]
		end
		obj:addTri(idx[1], idx[2], idx[3], tidx[1], tidx[2], tidx[3])
		obj:addTri(idx[1], idx[3], idx[4], tidx[1], tidx[3], tidx[4])
	end
end

local function jointLocal(rootCF: CFrame, part: BasePart, offset: Vector3): (number, number, number)
	local wp = part.CFrame:PointToWorldSpace(offset)
	local lp = rootCF:PointToObjectSpace(wp)
	return lp.X, lp.Y, lp.Z
end

local function buildJoints(character: Model, rootCF: CFrame): { { number } }
	local head = character:FindFirstChild("Head") :: BasePart
	local torso = character:FindFirstChild("Torso") :: BasePart
	local la = character:FindFirstChild("Left Arm") :: BasePart
	local ra = character:FindFirstChild("Right Arm") :: BasePart
	local ll = character:FindFirstChild("Left Leg") :: BasePart
	local rl = character:FindFirstChild("Right Leg") :: BasePart

	local function j(part: BasePart?, ox: number, oy: number, oz: number): { number }
		if not part then return { 0, 0, 0 } end
		local x, y, z = jointLocal(rootCF, part, Vector3.new(ox, oy, oz))
		return { x, y, z }
	end

	-- indices match JOINT_ORDER
	return {
		j(head, 0, 0, 0),
		j(torso, 0, torso and torso.Size.Y * 0.15 or 0, 0),
		j(la, 0, la and la.Size.Y * 0.35 or 0, 0),
		j(ra, 0, ra and ra.Size.Y * 0.35 or 0, 0),
		j(la, 0, 0, 0),
		j(ra, 0, 0, 0),
		j(la, 0, la and -la.Size.Y * 0.45 or 0, 0),
		j(ra, 0, ra and -ra.Size.Y * 0.45 or 0, 0),
		j(torso, 0, torso and -torso.Size.Y * 0.35 or 0, 0),
		j(ll, 0, ll and ll.Size.Y * 0.35 or 0, 0),
		j(rl, 0, rl and rl.Size.Y * 0.35 or 0, 0),
		j(ll, 0, ll and -ll.Size.Y * 0.45 or 0, 0),
		j(rl, 0, rl and -rl.Size.Y * 0.45 or 0, 0),
	}
end

local function exportR6(userId: number?)
	local model = loadR6(userId)
	local root = model:FindFirstChild("HumanoidRootPart") :: BasePart?
	if not root then
		error("[jewsploit] no HumanoidRootPart")
	end
	local rootCF = root.CFrame

	local obj = newObj()
	local vtCount = { 0 }

	table.insert(obj.lines, "# space: HumanoidRootPart local, Y-up, studs")
	table.insert(obj.lines, "# body parts only (no accessories)")

	local joints = buildJoints(model, rootCF)
	for i, name in JOINT_ORDER do
		local p = joints[i]
		table.insert(obj.lines, string.format("# joint %d %s %s %s %s", i - 1, name, fmt(p[1]), fmt(p[2]), fmt(p[3])))
	end
	table.insert(obj.lines, "")

	local n = 0
	for _, name in BODY do
		local part = model:FindFirstChild(name)
		if part and part:IsA("BasePart") then
			table.insert(obj.lines, "g " .. name:gsub(" ", "_"))
			addBox(obj, rootCF, part, vtCount)
			n += 1
		else
			warn("[jewsploit] missing part: " .. name)
		end
	end

	local text = table.concat(obj.lines, "\n") .. "\n"

	local parent = destParent()
	local old = parent:FindFirstChild("JewsploitModelOBJ")
	if old then old:Destroy() end
	local sv = Instance.new("StringValue")
	sv.Name = "JewsploitModelOBJ"
	sv.Value = text
	sv.Parent = parent

	print(string.format("[jewsploit] OK — %d body parts → %s", n, sv:GetFullName()))
	print("[jewsploit] Copy JewsploitModelOBJ.Value → save as model.obj → send it")
	pcall(function() Selection:Set({ sv }) end)
end

_G.JewsploitExportR6 = exportR6
print("[jewsploit] ready: _G.JewsploitExportR6()  or  _G.JewsploitExportR6(USER_ID)")
