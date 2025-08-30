function process_solver_logs(input_log_path, output_csv_path)
% process_solver_logs parses a single log file and appends the
% extracted metrics to a CSV file.
%
% Usage from the command line:
%   matlab -batch "process_solver_logs('input_log.txt', 'output.csv')"
%
% Args:
%   input_log_path (string): The path to the log file.
%   output_csv_path (string): The path to the output CSV file.

    % Define regular expressions for each metric
    patterns = struct(...
        'KLU_solve_status', 'KLU solve status: (\d+)', ...
        'Norm_of_the_residual', '2-Norm of the residual: ([\d.eE+-]+)', ...
        'FGMRES_error_estimation', 'FGMRES error estimation: ([\d.eE+-]+)', ...
        'FGMRES_final_nrm', 'FGMRES: init nrm: [\d.eE+-]+ final nrm: ([\d.eE+-]+)', ...
        'FGMRES_iterations', 'FGMRES: init nrm: [\d.eE+-]+ final nrm: [\d.eE+-]+ iter: (\d+)', ...
        'FGMRES_Effective_Stability', 'FGMRES Effective Stability: ([\d.eE+-]+)', ...
        'Relative_residual_after_error_update', 'Relative residual after error update: ([\d.eE+-]+)' ...
    );

    % Parse the log file
    try
        content = fileread(input_log_path);
    catch ME
        fprintf('Error: Could not read file at %s\n', input_log_path);
        return;
    end
    
    % Initialize a struct to hold the metrics
    metrics = struct();
    metric_names = fieldnames(patterns);
    
    for i = 1:length(metric_names)
        key = metric_names{i};
        pattern = patterns.(key);
        
        % Use regexp to find the value
        match = regexp(content, pattern, 'tokens', 'once');
        
        if ~isempty(match)
            % Convert to numeric if possible, otherwise keep as string
            if contains(key, 'status') || contains(key, 'iterations')
                metrics.(key) = str2double(match{1});
            else
                metrics.(key) = str2double(match{1});
            end
        else
            % If a metric is not found, set it to "Not Found"
            metrics.(key) = "Not Found";
        end
    end
    
    % Add the system number from the file name
    [~, name, ~] = fileparts(input_log_path);
    metrics.System = replace(name, {'solver_output_', '.txt'}, '');

    % Create a table from the metrics struct
    metrics_table = struct2table(metrics);

    % Write the table to CSV
    try
        if exist(output_csv_path, 'file') == 2
            % Append to the existing file
            writetable(metrics_table, output_csv_path, 'WriteMode', 'append');
        else
            % Create a new file with headers
            writetable(metrics_table, output_csv_path);
        end
        fprintf('Successfully wrote data for system %s to %s\n', metrics.System, output_csv_path);
    catch ME
        fprintf('Error: Could not write to file at %s\n', output_csv_path);
    end
end
