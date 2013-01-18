#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <string>
#include <set>

using namespace std;

string func_pattern; 
string void_func_pattern;
set<string> filter;

void init_filter()
{
  filter.clear();
#define add(x) filter.insert(#x)
  add(const);
  add(void);
  add(int);
  add(unsigned);
  add(pthread_mutex_t);
  add(restrict);
  add(pthread_mutexattr_t);
  add(struct);
  add(timespec);
  add(time_t);
  add(clockid_t);
  add(timeval);
  add(timezone);
  add(pthread_cond_t);
  add(sem_t);
  add(pthread_barrier_t);
  add(pthread_barrierattr_t);
  add(useconds_t);
  add(__useconds_t);
  add(sockaddr);
  add(size_t);
  add(ssize_t);
  add(pid_t);
  add(msghdr);
  add(socklen_t);
  add(epoll_event);
  add(pthread_rwlock_t);
#undef add
}

inline bool goodchar(char ch)
{
  return ch <= 'Z' && ch >= 'A'
    || ch <= 'z' && ch >= 'a'
    || ch <= '9' && ch >= '0'
    || ch == '_';
}

void replace(string &st, string pat, string tobe)
{
	while (true)
	{
		int p = st.find(pat);
		if (p == string::npos) return;
		st = st.replace(p, pat.size(), tobe);
	}
}

void print_func(
	FILE *file,
	const char *func_ret_type,
	const char *func_name, 
	const char *args_with_name, 
	const char *args_without_name,
	const char *args_only_name,
	const char *lib_path
	)
{
	string pattern = !strcmp(func_ret_type, "void") ? void_func_pattern : func_pattern;
	replace(pattern, "FUNC_RET_TYPE", func_ret_type);
	replace(pattern, "FUNC_NAME", func_name);
	replace(pattern, "ARGS_WITH_NAME", args_with_name);
	replace(pattern, "ARGS_WITHOUT_NAME", args_without_name);
	if (!strlen(args_only_name))
  {
    replace(pattern, ", ARGS_ONLY_NAME", args_only_name);
    replace(pattern, "ARGS_ONLY_NAME", args_only_name);
  } else
    replace(pattern, "ARGS_ONLY_NAME", args_only_name);
#if __WORDSIZE == 64
    std::string x86_64Path ="/lib/x86_64-linux-gnu";
    x86_64Path = x86_64Path + lib_path;
    replace(pattern, "LIB_PATH", x86_64Path.c_str());
#else
    std::string i686Path ="/lib";
    i686Path = i686Path + lib_path;
    replace(pattern, "LIB_PATH", i686Path.c_str());
#endif
	fprintf(file, "%s", pattern.c_str());
}

char buffer[1024];

bool read_line(FILE *fin, string &ret)
{
	while (true)
	{
		if (!fgets(buffer, 1024, fin)) return false;
		ret = buffer;
		ret.erase(ret.find_last_not_of(" \t\n\r\f\v") + 1);
		if (ret.size()) return true;
	}
}

void convert(FILE *fin)
{
  FILE *hook_cpp = fopen("template.cpp", "w");
  FILE *types = fopen("hook_type_def.h", "w");
/*
  fprintf(hook_cpp, "#include <pthread.h>\n");
	fprintf(hook_cpp, "#include <stdio.h>\n");
	fprintf(hook_cpp, "#include <dlfcn.h>\n");
	fprintf(hook_cpp, "#include <stdlib.h>\n");
	fprintf(hook_cpp, "#include <sys/types.h>\n");
	fprintf(hook_cpp, "#include <sys/socket.h>\n");
	fprintf(hook_cpp, "#include <sys/select.h>\n");
	fprintf(hook_cpp, "#include <sys/time.h>\n");
  fprintf(hook_cpp, "extern \"C\" void __gxx_personality_v0(void) {} \n\n");
  
	fprintf(hook_cpp, "#define PRINT_DEBUG\n");
*/
	while (true)
	{
		string flag = "";
		while (flag != "STARTDEF" && flag != "START_SHORT_DEFINE")
			if (!read_line(fin, flag)) break;

    string func_ret_type;
		string func_name;
		string args_with_name = "";
		string args_without_name = "";
		string args_only_name = "";
		string lib_path = "";
		if (flag == "STARTDEF")
    {		
  		if (!read_line(fin, func_ret_type)) break;
  		if (!read_line(fin, func_name)) break;
  		if (!read_line(fin, lib_path)) break;
  		while (read_line(fin, flag) && flag != "SECTION")
  			args_without_name += flag;
  		while (read_line(fin, flag) && flag != "SECTION")
  			args_with_name += flag;
  		while (read_line(fin, flag) && flag != "SECTION")
  			args_only_name += flag;
    } else
    if (flag == "START_SHORT_DEFINE")
    {
  		if (!read_line(fin, lib_path)) break;
  		if (!read_line(fin, func_ret_type)) break;
  		if (!read_line(fin, func_name)) break;
      string st;
  		if (!read_line(fin, st)) break;
      while (st[0] != '(') st = st.substr(1);
      while (st[st.size() - 1] != ')') st = st.substr(0, st.size() - 1);
      int len = st.size();
      st = st.substr(1, len - 2);
      args_with_name = st;
      args_only_name = "";
      args_without_name = "";
      do {
        string token = "";
        while (st.size() && goodchar(st[0]))
        {
          token = token + st[0];
          st = st.substr(1);
        }

        if (token.size())
          if (filter.find(token) == filter.end())
            args_only_name += token;
          else
            args_without_name += token;
        else {
          if (!st.size()) break;
          if (st[0] == ',')
          {
            args_only_name += ',';
            args_without_name += ',';
          } else
            args_without_name += st[0];
          st = st.substr(1);
        }
      } while (st.size());
    } else
      break;  		

    print_func(
  	  hook_cpp,
    	func_ret_type.c_str(), 
  		func_name.c_str(), 
  		args_with_name.c_str(), 
  		args_without_name.c_str(), 
  		args_only_name.c_str(), 
  		lib_path.c_str()
  	);


    fprintf(types, "typedef %s (*__%s_funcdef)(%s);\n", 
      func_ret_type.c_str(), 
      func_name.c_str(), 
      args_with_name.c_str());
	}
  fclose(hook_cpp);
  fclose(types);
}

string read_file(const char *name)
{
	FILE *file = fopen(name, "r");
	string ret = "";
	while (fgets(buffer, 1024, file))
	{
		ret = ret + buffer;
	}
	fclose(file);
	return ret;
}

int main()
{
  init_filter();
	func_pattern = read_file("func_template.cpp");
	void_func_pattern = read_file("void_func_template.cpp");
	convert(stdin);
	return 0;
}
