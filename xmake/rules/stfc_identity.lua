rule("stfc.identity")
    on_load(function(target)
        local function identity_define(option_name, fallback)
            local value = get_config(option_name)
            if value == nil then
                value = fallback
            end
            if type(value) ~= "string" or value == "" then
                raise(option_name .. " must not be empty")
            end
            if #value > 160 then
                raise(option_name .. " must not exceed 160 characters")
            end
            if not value:match("^[A-Za-z0-9%._:+/%-]+$") then
                raise(option_name .. " contains unsupported identity characters")
            end
            return value
        end

        target:add("defines", "STFC_SOURCE_STATE_ID=\"" ..
            identity_define("stfc_source_state_id", "unknown") .. "\"")
        target:add("defines", "STFC_BASE_COMMIT=\"" .. identity_define("stfc_base_commit", "unknown") .. "\"")
        target:add("defines", "STFC_BUILD_INVOCATION_ID=\"" ..
            identity_define("stfc_build_invocation_id", "xmake-direct") .. "\"")
        target:add("defines", "STFC_BUILD_MODE=\"" .. identity_define("mode", "unknown") .. "\"")
        target:add("defines", "STFC_BUILD_CHANNEL=\"" .. identity_define("stfc_build_channel", "local") .. "\"")
    end)
rule_end()
