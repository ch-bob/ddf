
#include <iostream>
#include <filesystem>
#include <getopt.h>
#include <unordered_map>
#include <fstream>
#include <vector>
#include <functional>

using namespace std;
namespace fs = std::filesystem;

#define LANGUAGE_SIMPLE_CHINESE // LANGUAGE_ENGLISH

#ifdef LANGUAGE_SIMPLE_CHINESE

unordered_map<int, string_view> dialogue = {
    {0, "是否删除重复文件?[Y/n]:"},
    {1, "跳过"},
    {2, "发现总文件数:"},
    {3, "重复文件数:"},
    {4, "文件流错误:"},
    {5, "至少输入一个有效路径作为参数"},
    {6, "路径错误"},
    {7, "路径不是目录类型"},
    {8, "帮助信息:\n\
示例:ddf <dir>  \n\
示例:ddf -r -f <dir>  \n\
可选项: \n\
-h  帮助信息 \n\
-r  目录递归执行 \n\
-f  强制删除不再询问 "},
    {9, "删除:"},
    {10, "未定义函数fun_PermitRequest"},
    {11, "遇到错误已退出当前程序"}};

#elifdef LANGUAGE_ENGLISH

unordered_map<int, string_view> dialogue = {
    {0, "Do you want to delete the file?[Y/n]:"},
    {1, "skip"},
    {2, "Number of files discovered:"},
    {3, "Number of duplicate files:"},
    {4, "File stream error:"},
    {5, "Enter at least one valid path as a parameter"},
    {6, "Path error"},
    {7, "The path is not of directory type"},
    {8, "Help Information:\n\
Usage:ddf <dir>  \n\
Usage:ddf -r -f <dir>  \n\
option: \n\
-h  Help Information \n\
-r  Recursive directory \n\
-f  Forcefully delete without further inquiry "},
    {9, "delete:"},
    {10, "Undefined function fun_CermitRequest"},
    {11, "Encountered an error and exited the current program"}

};

#endif

class DDF_Process
{
private:
    static const size_t bufferSize = 1 << 20; // 1MB
    array<char, bufferSize> buffer;
    unordered_map<uint32_t, fs::path> filePathHashMap;
    bool processStop = false;

    size_t filesCount = 0;

public:
    fs::path workDirectory;
    bool recursive = false;
    bool force = false;

    function<void(string_view)> fun_Output;                             // 错误输出
    function<bool(string_view, fs::directory_entry)> fun_PermitRequest; // 删除文件的权限许可

    //[0]成功 [-1]失败
    int ProcessStart()
    {
        if (!fs::exists(workDirectory))
        {
            fun_Output(dialogue[6]);
            return -1;
        }

        if (!fs::is_directory(workDirectory))
        {
            fun_Output(dialogue[7]);
            return -1;
        }

        if (fun_PermitRequest == nullptr && force == false)
        {
            fun_Output(dialogue[10]);
            return -1;
        }

        auto subprocess = [this](const fs::directory_entry &item)
        {
            if (!item.is_regular_file())
                return;
            filesCount++;

            ifstream in(item.path().c_str(), ios::in | ios::binary); // 打开byte模式输入
            if (!in.is_open())
            {
                fun_Output(string(dialogue[4]) + item.path().string());
                processStop = true;
                return;
            }

            in.seekg(0);
            in.read(buffer.data(), bufferSize);
            uint32_t h = calculate_hash(buffer.data(), in.gcount());
            in.close();
            if (filePathHashMap.find(h) != filePathHashMap.end()) // key已存在 进入删除流程
            {

                if (force) // 直接删
                {
                    fs::remove(item);
                    fun_Output(string(dialogue[9]) + item.path().string());
                }
                else // 请求许可 逐条询问
                {
                    bool req = fun_PermitRequest(dialogue[0], item);

                    if (req)
                    {
                        fs::remove(item);
                        fun_Output(string(dialogue[9]) + item.path().string());
                    }
                    else
                    {
                        fun_Output(dialogue[1]);
                    }
                }
            }
            else
            {
                filePathHashMap.insert(pair(h, item.path())); // key不存在,插入当前
            }
        };

        if (recursive)
        {
            auto dir = fs::recursive_directory_iterator(workDirectory);
            for (auto item : dir)
            {

                subprocess(item);
                if (processStop)
                    return -1;
            }
        }
        else
        {
            auto dir = fs::directory_iterator(workDirectory);
            for (auto item : dir)
            {
                subprocess(item);
                if (processStop)
                    return -1;
            }
        }

        return 0;
    }

    uint32_t calculate_hash(const char *bytes, size_t num)
    {
        uint32_t hash = 5381u;
        for (size_t i = 0; i < num; i++)
        {
            hash = ((hash << 5) + hash) ^ bytes[i];
        }

        return hash;
    }
    string generateSummaryReport()
    {
        stringstream ss;
        ss << dialogue[2] << filesCount << '\n';
        ss << dialogue[3] << filesCount - filePathHashMap.size();
        return ss.str();
    }
};

void help_and_die(char *argv0)
{
    cout << dialogue[8] << endl;

    exit(0);
}

int main(int argc, char **argv)
{

    const char *optstr = "hrf";
    char ch;

    bool option_flag_r = false; // recursive
    bool option_flag_f = false; // force

    while ((ch = getopt(argc, argv, optstr)) != -1)
    {
        switch (ch)
        {
        case 'h':
            help_and_die(argv[0]);
            exit(0);
            break;
        case 'r':
            option_flag_r = true;
            break;
        case 'f':
            option_flag_f = true;
            break;

        case '?':
            help_and_die(argv[0]);
            break;
        }
    }

    if (argv[optind] == nullptr)
    {
        cout << dialogue[5] << endl;
        help_and_die(argv[0]);
    }

    DDF_Process ddf;

    ddf.workDirectory = fs::path(argv[optind]);
    ddf.force = option_flag_f;
    ddf.recursive = option_flag_r;

    ddf.fun_Output = [](string_view sv)
    {
        cout << sv << endl;
    };
    ddf.fun_PermitRequest = [](string_view sv, const fs::directory_entry &item) -> bool
    {
        char c;
        cout << sv << item << endl;
        cin >> c;
        if (c == 'Y' || c == 'y')
            return true;
        else
            return false;
    };

    int res = ddf.ProcessStart();
    if (res != 0)
        cout << dialogue[11] << endl;
    else
        cout << ddf.generateSummaryReport() << endl;

    return 0;
}