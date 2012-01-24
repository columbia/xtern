testapps=bdb_rep
root_path=$(PWD)
EXE_PREFIX=

all: $(testapps)

$(testapps):
	cd $(root_path)/$@ && make eval EXE_PREFIX=$(EXE_PREFIX)
	
clean: 
	for app in $(testapps); do cd $(root_path)/$$app && make clean; done
