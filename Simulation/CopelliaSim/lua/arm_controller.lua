sim = require('sim')

local baseHandle = -1
local baseIsJoint = false
local baseParent = -1
local baseStartOrientation = {0, 0, 0}
local startTime = 0

local spinSpeedDegPerSec = 30
local spinSpeedRadPerSec = math.rad(spinSpeedDegPerSec)

local jointAliases = {
    'base_joint',
    'joint_1',
    'joint1',
    'BaseJoint',
}

local baseLinkAliases = {
    'link_00_base',
    'rotating_base',
}

local function log(message)
    sim.addLog(sim.verbosity_scriptinfos, message)
end

local function getAlias(handle)
    if sim.getObjectAlias then
        return sim.getObjectAlias(handle, -1)
    end
    return sim.getObjectName(handle)
end

local function normalizeName(name)
    name = string.lower(name)
    name = string.gsub(name, '[^a-z0-9]+', '_')
    name = string.gsub(name, '_+', '_')
    name = string.gsub(name, '^_', '')
    name = string.gsub(name, '_$', '')
    return name
end

local function findObjectByAlias(aliases, objectType)
    local wanted = {}
    for _, alias in ipairs(aliases) do
        wanted[normalizeName(alias)] = true

        local ok, handle = pcall(sim.getObject, '/' .. alias)
        if ok and handle and handle >= 0 then
            if not objectType or sim.getObjectType(handle) == objectType then
                return handle, getAlias(handle)
            end
        end
    end

    local objects = sim.getObjectsInTree(sim.handle_scene, sim.handle_all, 0) or {}
    for _, handle in ipairs(objects) do
        if not objectType or sim.getObjectType(handle) == objectType then
            local alias = getAlias(handle)
            if wanted[normalizeName(alias)] then
                return handle, alias
            end
        end
    end

    return -1, nil
end

function sysCall_init()
    local jointType = sim.sceneobject_joint or sim.object_joint_type
    local shapeType = sim.sceneobject_shape or sim.object_shape_type

    local alias
    baseHandle, alias = findObjectByAlias(jointAliases, jointType)
    baseIsJoint = baseHandle >= 0

    if not baseIsJoint then
        baseHandle, alias = findObjectByAlias(baseLinkAliases, shapeType)
    end

    if baseHandle < 0 then
        log('Base spin controller could not find base_joint/joint_1 or link_00_base.')
        return
    end

    startTime = sim.getSimulationTime()

    if baseIsJoint then
        log('Base spin controller driving joint: ' .. alias)
    else
        baseParent = sim.getObjectParent(baseHandle)
        baseStartOrientation = sim.getObjectOrientation(baseHandle, baseParent)
        log('Base spin controller rotating CAD link: ' .. alias)
    end
end

function sysCall_actuation()
    if baseHandle < 0 then
        return
    end

    local t = sim.getSimulationTime() - startTime
    local angle = math.fmod(t * spinSpeedRadPerSec, 2 * math.pi)

    if baseIsJoint then
        sim.setJointPosition(baseHandle, angle)
    else
        sim.setObjectOrientation(
            baseHandle,
            baseParent,
            {
                baseStartOrientation[1],
                baseStartOrientation[2],
                baseStartOrientation[3] + angle,
            }
        )
    end
end

function sysCall_cleanup()
    log('Base spin controller finished')
end
