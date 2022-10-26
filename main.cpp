#include <fstream>
#include <iostream>
#include <thread>
#include <regex>
#include <list>

using namespace std;

#define COUNT_THREADS 8

#pragma pack(push, 1)
struct OutputLine {
    OutputLine() = default;

    std::uint64_t number_of_place = 0;
    std::string found_line;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct InputLine {
    InputLine() = default;

    std::uint64_t number_of_line = 0;
    std::string line;
};
#pragma pack(pop)

auto exit_error = [](std::string message)
{
    cout << "Error!" << endl;
    cout << message << endl;
    cout << "For example: mtfind input.txt '?ad'" << endl;
    return 1;
};

std::mutex main_mutex;

auto main_lambda = [](bool& is_end_read,
                      uint64_t& count_of_found_items,
                      std::map<uint64_t, OutputLine>& main_list,
                      std::list<InputLine>& map_of_lines,
                      std::string reg_exp)
{
    std::regex self_regex("(" + reg_exp +")");

    // start loop
    while (true)
    {
        // default values
        std::string mm_local_string;
        uint64_t mm_number_of_line = 0;
        std::smatch match;

        main_mutex.lock();
        try
        {
            // checking end of program
            if (map_of_lines.empty() && is_end_read)
            {
                main_mutex.unlock();
                break;
            }
            // waiting new lines
            else if (map_of_lines.empty() && !is_end_read)
            {
                main_mutex.unlock();
                std::this_thread::sleep_for(10ms);
                continue;
            }
            // main work
            else
            {
                // get first line from memory
                mm_local_string = map_of_lines.begin()->line;
                mm_number_of_line = map_of_lines.begin()->number_of_line;
                map_of_lines.pop_front();
                main_mutex.unlock();

                // string search
                auto match_begin = std::sregex_iterator(mm_local_string.begin(), mm_local_string.end(), self_regex);
                auto match_end = std::sregex_iterator();

                if (match_begin != match_end)
                {
                    // cycle through all found items
                    for (std::sregex_iterator iter = match_begin; iter != match_end; ++iter)
                    {
                        // init output struct
                        OutputLine tmp_struct;
                        tmp_struct.found_line = iter->str();
                        tmp_struct.number_of_place = iter->position();

                        // change "global" values
                        main_mutex.lock();
                        count_of_found_items++;

                        auto it = std::find_if(main_list.begin(), main_list.end(),
                                               [&](const auto& item){return item.second.found_line == iter->str();});
                        // skipping same substrings
                        if (it != main_list.end())
                        {
                            main_mutex.unlock();
                            continue;
                        }

                        // if we find the second substring in the same line and it's unique, we can't insert it into the map and skip
                        // if you want to fix it, I can do it.
                        main_list.insert(std::pair(mm_number_of_line, tmp_struct));
                        main_mutex.unlock();
                    }
                }
            }
        }
        catch(const std::exception &ex)
        {
            main_mutex.unlock();
        }
    }
    return 1;
};

class main_executer : public std::thread {};

int main(int argc, char *argv[])
{
    if (argc != 3)
        return exit_error(std::string("You need to enter 2 parameters and now " +std::to_string(argc - 1) + "."));

    const std::string file_path = argv[1];
    std::string reg_exp = argv[2];

    // check input arguments
    {
        if (reg_exp.find('?') == std::string::npos)
            return exit_error(std::string("You don't have symbol '?' in second parameter."));

        if (reg_exp.find('\n') != std::string::npos)
            return exit_error(std::string("You can't have symbol '\\n' in second parameter."));

        if (reg_exp.size() > 1000)
            return exit_error(std::string("Your second parameter is too large."));
    }

    std::replace(reg_exp.begin(), reg_exp.end(), '?','.');

    std::ifstream file_stream(file_path);
    if (file_stream.is_open())
    {
        bool is_end_read = false;
        uint64_t count_of_found_items = 0;
        uint64_t count_of_line = 0;
        std::list<InputLine> map_of_lines;
        std::map<uint64_t /*number_of_line*/, OutputLine> main_list;

        // threads init
        std::vector<std::thread> threads_list(COUNT_THREADS);
        for (auto it = std::begin(threads_list); it != std::end(threads_list); ++it)
        {
            *it = std::thread(main_lambda, std::ref(is_end_read), std::ref(count_of_found_items), std::ref(main_list), std::ref(map_of_lines), reg_exp);
        }

        // reading the file
        std::string line;
        while (getline(file_stream, line))
        {
            count_of_line++;

            InputLine line_struct;
            line_struct.line = line;
            line_struct.number_of_line = count_of_line;

            main_mutex.lock();
            map_of_lines.emplace_back(line_struct);
            main_mutex.unlock();
        }
        is_end_read = true;

        // wait threads
        for(auto& i : threads_list) {
            i.join();
        }

        // output
        cout << count_of_found_items << endl;
        for (auto& item : main_list)
        {
            cout << std::to_string(item.first) << " " << std::to_string(item.second.number_of_place) << " " << item.second.found_line << endl;
        }

    }
    else
        return exit_error("File opening failed. You need copy file to next to programm.");

    file_stream.close();
    return 0;
}
