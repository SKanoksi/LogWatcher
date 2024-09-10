/******************************************************************
*
*  logwatcher
*
*  Constantly monitor a log file and watch out for specific keywords,
*  if any appears, then execute the assigned command.
*
*  Copyright (c) 2024, Somrath Kanoksirirath.
*  All rights reserved under BSD 3-clause license.
*
******************************************************************/

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <memory>
#include <stdexcept>
#include <array>
#include <thread>
#include <algorithm>
#include <unistd.h> // getpid, getppid and fork -- LINUX host

#include "logwatcher.hpp"


void string_toupper(std::string &s)
{
    //From https://en.cppreference.com/w/cpp/string/byte/toupper
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::toupper(c); } );
return; }


bool file_exists(const std::string &filename)
{
    std::ifstream file(filename.c_str());

return file.good(); }


std::string get_timestamp()
{
    auto current_time = std::chrono::system_clock::now() ;
    std::time_t current_time_t = std::chrono::system_clock::to_time_t(current_time) ;

return std::ctime(&current_time_t); }


std::string system_exec(const char* cmd)
{
    // From https://stackoverflow.com/questions/478898/how-do-i-execute-a-command-and-get-the-output-of-the-command-within-c-using-po
    std::array<char,128> buffer ;
    std::string result ;
    std::unique_ptr<FILE,decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if( !pipe ){
        throw std::runtime_error("popen() failed!");
    }
    while( fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr ){
        result += buffer.data();
    }

return result; }


inline void main_execute(const std::string &command, const std::string &exe_uppername)
{
    if( std::system(command.c_str()) == 0 ){
        std::cout << exe_uppername << ":: Command executed successfully" << std::endl;
    }else{
        std::cerr
        << exe_uppername << "::ERROR -- Abnomal return when executing the command \""
        << command
        << "\", won't repeat." << std::endl;
    }

return; }


int main(int argc, char *argv[])
{
    // INIT
    long long int interval_seconds = DEFAULT_INTERVAL_SECOND ;
    long long int timeout_seconds  = -1 ;
    bool found_cond = true ;
    bool all_cond = false ;
    bool check_once = false ;
    bool check_at_start = false ;
    bool stay = false ;
    bool verbose = false ;
    bool run_in_background = true ;
    int ppid = -1 ;
    std::string check_ppid_alive ;
    bool start   = true  ;
    bool error_exit = true ;

    std::string filepath ;
    std::vector<std::string> keywords ;
    std::string command ;


    // START parsing arguments
    executable_name = argv[0] ;
    std::size_t pos = executable_name.find_last_of('/');
    if( pos != std::string::npos ){
        executable_name = executable_name.substr(pos+1);
    }
    std::string executable_uppername = executable_name ;
    string_toupper(executable_uppername);

    const std::string ignore_keyword = "<" + executable_uppername + "-ignore>" ;

    for(int i=1 ; i<argc ; ++i)
    {
        std::string arg = argv[i] ;

        if( arg=="-f" || arg=="--logfile" ){
            if( i+1<argc ){
                //if( file_exists(argv[i+1]) ){  // Note: delayed to the first check to allow software log
                filepath = argv[i+1];
                ++i ;
            }else{
                std::cerr << executable_uppername << "::ERROR -- Option -f,--logfile requires a file path." << std::endl;
                start = false ;
                break;
            }
        }else if( arg=="-c" || arg=="--catch" ){
            if( i+1<argc ){
                keywords.push_back( argv[i+1] );
                ++i ;
            }else{
                std::cerr << executable_uppername << "::ERROR -- Option -c,--catch requires a text message (use multiple -c,--catch for several keywords)."<< std::endl;
                start = false ;
                break;
            }
        }else if( arg=="-e" || arg=="--execute" ){
            if( i+1<argc ){
                command = argv[i+1] ;
                ++i ;
            }else{
                std::cerr << executable_uppername << "::ERROR -- Option -e,--execute requires a command." << std::endl;
                start = false ;
                break;
            }
        }else if( arg=="-n" || arg=="--interval" ){
            if( i+1<argc ){
                interval_seconds = abs(std::stoll( argv[i+1] ));
                if( interval_seconds == 0 ){
                    interval_seconds = DEFAULT_INTERVAL_SECOND ;
                    std::cout << executable_uppername << "::WARNING -- Invalid input for option -n,--interval, will use the default value." << std::endl;
                }
                ++i ;
            }else{
                std::cerr << executable_uppername << "::ERROR -- Option -n,--interval requires an integer number." << std::endl;
                start = false ;
                break;
            }
        }else if( arg=="-t" || arg=="--timeout" ){
            if( i+1<argc ){
                timeout_seconds = abs(std::stoll( argv[i+1] ))*60 ;
                if( timeout_seconds <= 0 ){
                    timeout_seconds = MAX_TIMEOUT_SECOND ;
                    std::cout << executable_uppername << "::WARNING -- Invalid input for option -t,--timeout, will set to the longest." << std::endl;
                }
                ++i ;
            }else{
                std::cerr << executable_uppername << "::ERROR -- Option -t,--timeout requires an integer number." << std::endl;
                start = false ;
                break;
            }
        }else if( arg=="--missing" ){
            found_cond = false ;
        }else if( arg=="--all" ){
            all_cond = true ;
        }else if( arg=="--stay" ){
            stay = true ;
        }else if( arg=="--check-once" ){
            check_once = true ;
        }else if( arg=="--check-at-start" ){
            check_at_start = true ;
        }else if( arg=="--foreground" ){
            run_in_background = false ;
        }else if( arg=="-v" || arg=="--verbose" ){
            verbose = true ;
        }else if( arg=="-V" || arg=="--version" ){
            std::cout << version_string << std::endl;
            start = false ;
            error_exit = false ;
            break;
         }else if( arg=="-h" || arg=="--help" ){
            print_usage(executable_name);
            start = false ;
            error_exit = false ;
            break;
        }else if( arg=="--ppid" ){
            if( i+1<argc ){
                if( ppid<0 ) // Accept the first, quietly reject others
                {
                    ppid = std::stoi( argv[i+1] ) ;
                }
                ++i ;
            }else{
                std::cerr << executable_uppername << "::ERROR -- Option --ppid requires the parent PID, i.e., ${PPID}." << std::endl;
                start = false ;
                break;
            }
        }else{
            print_usage(executable_name);
            std::cerr << executable_uppername << "::ERROR -- Unknown input argument " << arg << std::endl;
            start = false ;
            break;
        }
    }
    if( !start ){ return (error_exit) ? 1:0 ; }
    // END parsing arguments


    // Initial check and assign
    start = set_defaults_and_startup_cond();

    if( interval_seconds < MIN_INTERVAL_SECOND ){
        interval_seconds = MIN_INTERVAL_SECOND ;
        std::cout << executable_uppername << "::WARNING -- Input interval was shorter than permitted. Set to the shortest allowed." << std::endl;
    }
    if( interval_seconds > MAX_INTERVAL_SECOND && !check_once ){
        interval_seconds = MAX_INTERVAL_SECOND ;
        std::cout << executable_uppername << "::WARNING -- Input interval was longer than permitted. Set to the longest allowed." << std::endl;
    }
    if( interval_seconds > MAX_TIMEOUT_SECOND && check_once ){
        interval_seconds = MAX_TIMEOUT_SECOND ;
        std::cout << executable_uppername << "::WARNING -- Input interval was longer than permitted. Set to the longest allowed." << std::endl;
    }
    if( timeout_seconds < 0 ){
        timeout_seconds = MAX_TIMEOUT_SECOND  ;
    }else{
        if( timeout_seconds > MAX_TIMEOUT_SECOND ){
            timeout_seconds = MAX_TIMEOUT_SECOND  ;
            std::cout << executable_uppername << "::WARNING -- Input timeout was longer than permitted. Set to the longest allowed." << std::endl;
        }
    }
    if( check_once ){
        if( check_at_start ){
            timeout_seconds = 1 ;
        }else{
            timeout_seconds = interval_seconds + 1 ;
        }
        if( stay ){
            stay = false ;
            std::cout << executable_uppername << "::WARNING -- The --check-once option is set, so the --stay option will be ignored." << std::endl;
        }
    }

    if( filepath.empty() ){ filepath = DEFAULT_FILEPATH ; }
    if( filepath.empty() ){
        std::cerr << executable_uppername << "::ERROR -- No filepath was specified. The option, --logfile <filepath>, is needed to be specified manually." << std::endl;
        return 1;
    }
    // Note: Won't check whether file exists here --> permit delay for ~interval_seconds

    if( keywords.empty() ){ keywords = DEFAULT_KEYWORDS ; }
    if( keywords.empty() ){
        std::cerr << executable_uppername << "::ERROR -- No keyword was specified. The option, --catch <keyword>, is needed to be specified manually." << std::endl;
        return 1;
    }

    if( command.empty() ){ command = DEFAULT_COMMAND ; }
    if( command.empty() ){
        std::cerr << executable_uppername << "::ERROR -- No command was specified. The option, --execute <command>, is needed to be specified manually." << std::endl;
        return 1;
    }else{
        if( command.find(executable_name) != std::string::npos ){
            if( stay ){
                std::cerr
                << executable_uppername << "::ERROR -- Possible recursive invocation with --stay option set!! '"
                << executable_name << "' appears in the specified command. Terminate." << std::endl;
                return 1;
            }else{
                std::cout
                << executable_uppername << "::WARNING -- Possible recursive invocation!! '"
                << executable_name << "' appears in the specified command." << std::endl;
            }
        }
        std::string check_command = "command -v " + command + " >/dev/null 2>&1" ;
        if( std::system(check_command.c_str()) != 0 ){
            std::cerr << executable_uppername << "::ERROR -- The specified command, \"" << command << "\", seem to be invalid. Please recheck it carefully." << std::endl;
            return 1;
        }
    }

    if( ppid < 0 ){
        ppid = static_cast<int>( getppid() );
    }
    if( ppid > 0 ){
        check_ppid_alive = "ps -o pid= -p " + std::to_string(ppid) ;
    }else{
        if( CHECK_PPID ){
            std::cerr << executable_uppername << "::ERROR -- Cannot get the parent process ID. The option, --ppid ${PPID}, is needed to be specified manually." << std::endl;
            return 1;
        }
    }

    // FORK process
    if( run_in_background ){
        pid_t cpid = fork(); // Create child process
        if( cpid<0 ){
            perror("SYSTEM::ERROR -- Cannot fork child process.\n");
            return 1;
        }
        if( cpid>0 ){ // Parent process terminate
            return 0;
        }
    }


    //  --- --- ---  MAIN PROGRAM --- --- ---


    // Prepare string for display
    std::string keyword_list ;
    if( keywords.size() > 1 ){
        keyword_list += " \"" + keywords[0] + "\"" ;
        for(unsigned int i=1 ; i<keywords.size()-1 ; ++i){
            keyword_list += ", \"" + keywords[i] + "\"" ;
        }
        if( all_cond ){
            keyword_list += " and \"" + keywords[keywords.size()-1] + "\" " ;
        }else{
            keyword_list += " or \"" + keywords[keywords.size()-1] + "\" " ;
        }
    }else{
        keyword_list = " \"" + keywords[0] + "\" " ;
    }

    // Get PID and hostname
    pid_t pid = getpid();
    char hostname[256];
    if( gethostname(hostname, sizeof(hostname)) != 0 ){
        std::cerr << executable_uppername << "::ERROR -- Cannot get hostname -- Is this a Linux host?" << std::endl;
        return 1;
    }

    // Display setup
    std::cout
    << "--- " << executable_uppername << " --- \n"
    << " PID " << pid << " on " << hostname << "\n"
    << " Monitor \"" << filepath << "\" \n"
    << " Every " << interval_seconds << " seconds\n"
    << " It will execute \"" << command << "\", " << std::endl;
    if( found_cond ){
        if( keywords.size()>1 ){
            if( all_cond ){
                std::cout << " If" << keyword_list << "are all found. -- " << ignore_keyword << std::endl;
            }else{
                std::cout << " If any of" << keyword_list << "is found. -- " << ignore_keyword << std::endl;
            }
        }else{
            std::cout << " If" << keyword_list << "is found. -- " << ignore_keyword << std::endl;
        }
    }else{
        if( keywords.size()>1 ){
            if( all_cond ){
                std::cout << " If" << keyword_list << "are all missing. -- " << ignore_keyword << std::endl;
            }else{
                std::cout << " If any of" << keyword_list << "is missing. -- " << ignore_keyword << std::endl;
            }
        }else{
            std::cout << " If" << keyword_list << "is missing. -- " << ignore_keyword << std::endl;
        }
    }
    if( timeout_seconds%60 == 0 ){
        std::cout
        << " Timeout in ~" << int(timeout_seconds/60) << " minutes\n"
        << "--- " << executable_uppername << " --- " << std::endl;
    }else{
        std::cout
        << " Timeout in ~" << int(timeout_seconds/60) + 1 << " minutes\n"
        << "--- " << executable_uppername << " --- " << std::endl;
    }


    // --- Start up conditions ---

    if( !start ){
        std::cerr << executable_uppername << "::ERROR -- " << FALSE_START_MESSAGE << std::endl;
        return 1 ;
    }

    // --- START MAIN LOOP ---

    long long int duration = 0 ;
    if( !check_at_start ){
        std::this_thread::sleep_for(std::chrono::seconds(interval_seconds));
        duration = interval_seconds ;
    }

    error_exit  = false ;
    bool condition = false ;
    std::vector<bool> found(keywords.size(), false);

    for( ; duration <= timeout_seconds ; duration += interval_seconds)
    {
        // Parent process existence check
        if( CHECK_PPID ){
            std::string output = system_exec(check_ppid_alive.c_str());
            if( output.empty() ){
                std::cerr
                << executable_uppername << ":: Parent process (PPID=" << std::to_string(ppid)
                << ") does NOT exist at " << get_timestamp()
                << executable_uppername << ":: Terminate." << std::endl;
                error_exit = true ;
                break;
            }
        }

        if( file_exists(filepath) )
        {
            for(unsigned int num=0 ; num < found.size() ; ++num){
                found[num] = false ;
            }
            std::ifstream file(filepath);
            std::string line, the_keyword ;
            while( std::getline(file, line) )
            {
                for(unsigned int num=0 ; num < keywords.size() ; ++num){
                    if( line.find(keywords[num]) != std::string::npos && line.find(ignore_keyword) == std::string::npos ){
                        found[num] = true ;
                    }
                }
            }
            file.close();

            if( found_cond ){
                if( all_cond ){
                    condition = std::all_of(found.begin(), found.end(), [](bool x) { return x; });
                    if( condition ){
                        std::cout
                        << executable_uppername << ":: All keywords are found at " << get_timestamp()
                        << executable_uppername << ":: Execute \"" << command << "\"" << std::endl;
                        main_execute(command, executable_uppername);
                    }else if( verbose ){
                        std::cout << executable_uppername << ":: One or more keywords were missing at " << get_timestamp() << std::flush;
                    }
                }else{
                    condition = std::any_of(found.begin(), found.end(), [](bool x) { return x; });
                    if( condition ){
                        std::cout
                        << executable_uppername << ":: One or more keywords are found at " << get_timestamp()
                        << executable_uppername << ":: Execute \"" << command << "\"" << std::endl;
                        main_execute(command, executable_uppername);
                    }else if( verbose ){
                        std::cout << executable_uppername << ":: All keywords were missing at " << get_timestamp() << std::flush;
                    }
                }
            }else{
                if( all_cond ){
                    condition = std::all_of(found.begin(), found.end(), [](bool x) { return !x; });
                    if( condition ){
                        std::cout
                        << executable_uppername << ":: All keywords are missing at " << get_timestamp()
                        << executable_uppername << ":: Execute \"" << command << "\"" << std::endl;
                        main_execute(command, executable_uppername);
                    }else if( verbose ){
                        std::cout << executable_uppername << ":: One or more keywords were found at " << get_timestamp() << std::flush;
                    }
                }else{
                    condition = std::any_of(found.begin(), found.end(), [](bool x) { return !x; });
                    if( condition ){
                        std::cout
                        << executable_uppername << ":: One ore more keywords are missing at " << get_timestamp()
                        << executable_uppername << ":: Execute \"" << command << "\"" << std::endl;
                        main_execute(command, executable_uppername);
                    }else if( verbose ){
                        std::cout << executable_uppername << ":: All keywords were found at " << get_timestamp() << std::flush;
                    }
                }
            }

        }else{
            std::cerr
            << executable_uppername << "::ERROR -- The file \"" << filepath << "\" does NOT exist at " << get_timestamp()
            << executable_uppername << ":: Terminate." << std::endl;
            error_exit = true ;
            break;
        }

        // Permit executing the command repeatedly --> for monitoring purpose
        if( condition && !stay ){ break; }

        std::this_thread::sleep_for(std::chrono::seconds(interval_seconds));

    } // FOR MAIN LOOP


    if( !condition && !error_exit && verbose ){
        std::cerr
        << executable_uppername << ":: Timeout reached.\n"
        << executable_uppername << ":: Terminate." << std::endl;
    }


return (error_exit) ? 1:0 ; }

