function sysCall_init()
    sim.addLog(sim.verbosity_scriptinfos, "arm_controller.lua loaded")
end

function sysCall_actuation()
    -- Add joint control logic here.
end

function sysCall_sensing()
    -- Add sensing or state estimation here.
end

function sysCall_cleanup()
    sim.addLog(sim.verbosity_scriptinfos, "arm_controller.lua finished")
end
