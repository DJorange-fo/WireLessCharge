%% build_wireless_charge_model.m
%% ?????? Simulink ??????
%% ????:
%%   1. ? MATLAB ??? wireless_charge_params.m ?????
%%   2. ??????? Simulink ??
%%
%% ??: Series-Series (SS) ??

if ~exist("f_sw", "var")
    error("???? wireless_charge_params.m ??????");
end

model_name = "wireless_charge_model";

if bdIsLoaded(model_name)
    close_system(model_name, 0);
end

new_system(model_name);
open_system(model_name);

fprintf("??????????????...\n");

%% ==================== ????: ????? ====================
add_block("simulink/Sources/Signal Generator", ...
    strcat(model_name, "/Inverter_SquareWave"), ...
    "Position", [100, 150, 200, 200], ...
    "WaveForm", "square", ...
    "Amplitude", "V_dc", ...
    "Frequency", "f_sw");

%% ==================== ????: ?? RLC ???? ====================
%% I_primary(s)/V_inv(s) = (s/Lp) / (s^2 + (Rp/Lp)*s + 1/(Lp*Cp))
num_primary = [1/L_p, 0];
den_primary = [1, R_p/L_p, 1/(L_p*C_p)];

add_block("simulink/Continuous/Transfer Fcn", ...
    strcat(model_name, "/Primary_Resonance"), ...
    "Position", [290, 140, 400, 210], ...
    "Numerator", mat2str(num_primary), ...
    "Denominator", mat2str(den_primary));

%% ==================== ????: ???? ====================
%% V_ind = M * d(I_primary)/dt

add_block("simulink/Continuous/Derivative", ...
    strcat(model_name, "/Derivative_dIp_dt"), ...
    "Position", [470, 160, 520, 190]);

add_block("simulink/Math Operations/Gain", ...
    strcat(model_name, "/Mutual_Inductance_M"), ...
    "Position", [570, 160, 620, 190], ...
    "Gain", "M");

%% ==================== ????: ?? RLC ???? ====================
%% I_secondary(s)/V_ind(s) = (s/Ls) / (s^2 + (Rs/Ls)*s + 1/(Ls*Cs))
num_secondary = [1/L_s, 0];
den_secondary = [1, R_s/L_s, 1/(L_s*C_s)];

add_block("simulink/Continuous/Transfer Fcn", ...
    strcat(model_name, "/Secondary_Resonance"), ...
    "Position", [690, 140, 800, 210], ...
    "Numerator", mat2str(num_secondary), ...
    "Denominator", mat2str(den_secondary));

%% ==================== ????: ???? + ?? ====================
%% V_load = abs(I_secondary) * R_load

add_block("simulink/Math Operations/Abs", ...
    strcat(model_name, "/Rectifier_ABS"), ...
    "Position", [870, 160, 920, 190]);

add_block("simulink/Math Operations/Gain", ...
    strcat(model_name, "/Load_Resistor"), ...
    "Position", [960, 160, 1010, 190], ...
    "Gain", "R_load");

%% ==================== ????: ????? ====================

add_block("simulink/Sinks/Scope", ...
    strcat(model_name, "/Scope_Output"), ...
    "Position", [1070, 130, 1140, 220], ...
    "NumInputPorts", "3");

add_block("simulink/Sinks/To Workspace", ...
    strcat(model_name, "/Ip_workspace"), ...
    "Position", [440, 290, 510, 320], ...
    "VariableName", "I_primary_out", ...
    "SaveFormat", "Array");

add_block("simulink/Sinks/To Workspace", ...
    strcat(model_name, "/Is_workspace"), ...
    "Position", [760, 290, 830, 320], ...
    "VariableName", "I_secondary_out", ...
    "SaveFormat", "Array");

add_block("simulink/Sinks/To Workspace", ...
    strcat(model_name, "/Vload_workspace"), ...
    "Position", [960, 290, 1030, 320], ...
    "VariableName", "V_load_out", ...
    "SaveFormat", "Array");

%% ==================== ????? ====================

add_line(model_name, "Inverter_SquareWave/1", "Primary_Resonance/1");
add_line(model_name, "Primary_Resonance/1", "Derivative_dIp_dt/1");
add_line(model_name, "Derivative_dIp_dt/1", "Mutual_Inductance_M/1");
add_line(model_name, "Mutual_Inductance_M/1", "Secondary_Resonance/1");
add_line(model_name, "Secondary_Resonance/1", "Rectifier_ABS/1");
add_line(model_name, "Rectifier_ABS/1", "Load_Resistor/1");
add_line(model_name, "Load_Resistor/1", "Scope_Output/3");
add_line(model_name, "Primary_Resonance/1", "Scope_Output/1");
add_line(model_name, "Primary_Resonance/1", "Ip_workspace/1");
add_line(model_name, "Secondary_Resonance/1", "Scope_Output/2");
add_line(model_name, "Secondary_Resonance/1", "Is_workspace/1");
add_line(model_name, "Load_Resistor/1", "Vload_workspace/1");

%% ==================== ???? ====================

set_param(model_name, "Solver", "ode23tb");
set_param(model_name, "StartTime", "0");
set_param(model_name, "StopTime", "T_sim");
set_param(model_name, "MaxStep", "Ts");
set_param(model_name, "RelTol", "1e-5");
set_param(model_name, "AbsTol", "1e-7");

%% ==================== ???? ====================

script_dir = fileparts(mfilename("fullpath"));
save_path = fullfile(script_dir, strcat(model_name, ".slx"));

save_system(model_name, save_path);
fprintf("\n??????: %s\n", save_path);

%% ??????????
model_workspace = get_param(model_name, "ModelWorkspace");
var_list = who;
for i = 1:length(var_list)
    if ismember(var_list{i}, {"f_sw", "V_dc", "L_p", "L_s", "C_p", "C_s", ...
            "M", "R_p", "R_s", "R_load", "T_sim", "T_sw", "Ts", "omega_sw", ...
            "k", "duty", "phase_shift", "C_dc"})
        model_workspace.assignin(var_list{i}, eval(var_list{i}));
    end
end

fprintf("???????\n");

%% ==================== ???? ====================
fprintf("\n??????...\n");
try
    simOut = sim(model_name, "SimulationMode", "normal");
    
    figure("Name", "??????????", "NumberTitle", "off");
    
    subplot(3,1,1);
    plot(simOut.get("I_primary_out").time, simOut.get("I_primary_out").data);
    xlabel("?? (s)"); ylabel("???? (A)");
    title("??????");
    grid on;
    
    subplot(3,1,2);
    plot(simOut.get("I_secondary_out").time, simOut.get("I_secondary_out").data);
    xlabel("?? (s)"); ylabel("???? (A)");
    title("??????");
    grid on;
    
    subplot(3,1,3);
    plot(simOut.get("V_load_out").time, simOut.get("V_load_out").data);
    xlabel("?? (s)"); ylabel("???? (V)");
    title("??????");
    grid on;
    
    sgtitle("?????? - Series-Series ????????");
    
    fprintf("\n===== ????? =====\n");
catch ME
    fprintf("\n??????: %s\n", ME.message);
end
