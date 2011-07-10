LEVEL := ../..
TOOLNAME = tern-instr

LINK_COMPONENTS := bitreader bitwriter asmparser scalaropts
USEDLIBS := recinstr.a commoninstr.a common.a

include $(LEVEL)/Makefile.common

# FIXME: better way to link in our libraries in llvm/install/lib
LIBS += $(LLVM_ROOT)/install/lib/libid-manager.a \
	$(LLVM_ROOT)/install/lib/libLLVMSupport.a
