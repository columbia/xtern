#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <string>

using namespace std;

string func_pattern; 
string void_func_pattern;

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
	replace(pattern, "ARGS_ONLY_NAME", args_only_name);
	replace(pattern, "LIB_PATH", lib_path);
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
		while (flag != "STARTDEF")
			if (!read_line(fin, flag)) break;
		if (flag != "STARTDEF") break;
		
		string func_ret_type;
		string func_name;
		string args_with_name = "";
		string args_without_name = "";
		string args_only_name = "";
		string lib_path = "";
		if (!read_line(fin, func_ret_type)) break;
		if (!read_line(fin, func_name)) break;
		if (!read_line(fin, lib_path)) break;
		while (read_line(fin, flag) && flag != "SECTION")
			args_without_name += flag;
		while (read_line(fin, flag) && flag != "SECTION")
			args_with_name += flag;
		while (read_line(fin, flag) && flag != "SECTION")
			args_only_name += flag;
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
	func_pattern = read_file("func_template.cpp");
	void_func_pattern = read_file("void_func_template.cpp");
	convert(stdin);
	return 0;
}
