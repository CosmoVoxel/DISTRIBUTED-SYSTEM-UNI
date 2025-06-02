# ============================================================================
# MPI Development Makefile
# ============================================================================
# A comprehensive Makefile for MPI C++ development with all essential features
# ============================================================================

# Compiler and tools
CC          := mpicxx
DEBUGGER    := gdb
VALGRIND    := valgrind

# Project structure
SRCDIR      := .
OBJDIR      := obj
BINDIR      := bin
SOURCES     := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS     := $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
TARGET      := $(BINDIR)/main.out

# Compiler flags
CFLAGS      := -Wall -Wextra -std=c++23
DEBUGFLAGS  := -g -O0 -DDEBUG
RELEASEFLAGS := -O2 -DNDEBUG
INCLUDES    := -I$(SRCDIR)

# MPI Runtime parameters
NP          := 6
HOSTFILE    := 
MPIFLAGS    := 

# Default build mode (debug or release)
BUILD_MODE  := debug

# Conditional compilation flags
ifeq ($(BUILD_MODE),debug)
    CFLAGS += $(DEBUGFLAGS)
else ifeq ($(BUILD_MODE),release)
    CFLAGS += $(RELEASEFLAGS)
else
    $(error Invalid BUILD_MODE: $(BUILD_MODE). Use 'debug' or 'release')
endif

# Color codes for output
RED         := \033[31m
GREEN       := \033[32m
YELLOW      := \033[33m
BLUE        := \033[34m
MAGENTA     := \033[35m
CYAN        := \033[36m
RESET       := \033[0m

# ============================================================================
# Main targets
# ============================================================================

.PHONY: all debug release clean run test install check help info buildrun
.DEFAULT_GOAL := help

# Build with current mode
all: $(TARGET)

# Debug build
debug:
	@$(MAKE) BUILD_MODE=debug $(TARGET)

# Release build  
release:
	@$(MAKE) BUILD_MODE=release $(TARGET)

# Create directories and build target
$(TARGET): $(OBJECTS) | $(BINDIR)
	@echo -e "$(GREEN)Linking$(RESET) $(notdir $@)..."
	@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo -e "$(GREEN)Build complete:$(RESET) $@"

# Compile source files to object files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	@echo -e "$(BLUE)Compiling$(RESET) $(notdir $<)..."
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Create necessary directories
$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(BINDIR):
	@mkdir -p $(BINDIR)

# ============================================================================
# Development and testing targets
# ============================================================================

# Build and run
buildrun: $(TARGET)
	@$(MAKE) run

# Run the program
run: $(TARGET)
	@echo -e "$(CYAN)Running with $(NP) processes...$(RESET)"
	@if [ -n "$(HOSTFILE)" ]; then \
		echo -e "$(YELLOW)Using hostfile:$(RESET) $(HOSTFILE)"; \
		mpirun -np $(NP) -hostfile $(HOSTFILE) $(MPIFLAGS) ./$(TARGET); \
	else \
		mpirun -np $(NP) $(MPIFLAGS) ./$(TARGET); \
	fi

# Run with custom number of processes
run-np:
	@if [ -z "$(N)" ]; then \
		echo -e "$(RED)Error:$(RESET) Please specify number of processes: make run-np N=<number>"; \
		exit 1; \
	fi
	@echo -e "$(CYAN)Running with $(N) processes...$(RESET)"
	@mpirun -np $(N) $(MPIFLAGS) ./$(TARGET)

# Debug with GDB
debug-run: debug
	@echo -e "$(YELLOW)Starting GDB session...$(RESET)"
	@mpirun -np $(NP) xterm -e gdb ./$(TARGET)

# Memory check with Valgrind
memcheck: debug
	@echo -e "$(YELLOW)Running memory check with Valgrind...$(RESET)"
	@mpirun -np $(NP) $(VALGRIND) --tool=memcheck --leak-check=full --show-leak-kinds=all ./$(TARGET)

# Test different process counts
test: $(TARGET)
	@echo -e "$(CYAN)Testing with different process counts...$(RESET)"
	@for np in 2 4 6 8; do \
		echo -e "$(YELLOW)Testing with $$np processes:$(RESET)"; \
		mpirun -np $$np ./$(TARGET) || true; \
		echo ""; \
	done

# Quick syntax check
check:
	@echo -e "$(BLUE)Performing syntax check...$(RESET)"
	@$(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only $(SOURCES)
	@echo -e "$(GREEN)Syntax check passed!$(RESET)"

# ============================================================================
# Utility targets
# ============================================================================

# Clean build artifacts
clean:
	@echo -e "$(RED)Cleaning up...$(RESET)"
	@rm -rf $(OBJDIR) $(BINDIR) *.out *.o core.*
	@echo -e "$(GREEN)Clean complete!$(RESET)"

# Deep clean (including backup files)
distclean: clean
	@echo -e "$(RED)Deep cleaning...$(RESET)"
	@rm -f *~ *.bak *.swp .*.swp
	@echo -e "$(GREEN)Deep clean complete!$(RESET)"

# Install MPI if not present (Ubuntu/Debian)
install-mpi:
	@echo -e "$(YELLOW)Installing MPI development tools...$(RESET)"
	@sudo apt-get update && sudo apt-get install -y mpich libmpich-dev

# Show project information
info:
	@echo -e "$(CYAN)Project Information:$(RESET)"
	@echo -e "  $(BLUE)Compiler:$(RESET)     $(CC)"
	@echo -e "  $(BLUE)Build Mode:$(RESET)   $(BUILD_MODE)"
	@echo -e "  $(BLUE)Flags:$(RESET)        $(CFLAGS)"
	@echo -e "  $(BLUE)Sources:$(RESET)      $(SOURCES)"
	@echo -e "  $(BLUE)Target:$(RESET)       $(TARGET)"
	@echo -e "  $(BLUE)Processes:$(RESET)    $(NP)"
	@if [ -n "$(HOSTFILE)" ]; then \
		echo -e "  $(BLUE)Hostfile:$(RESET)     $(HOSTFILE)"; \
	fi

# Show help
help:
	@echo -e "$(CYAN)MPI Development Makefile$(RESET)"
	@echo -e "$(YELLOW)Available targets:$(RESET)"
	@echo -e "  $(GREEN)Build targets:$(RESET)"
	@echo -e "    all         - Build with current mode (default: debug)"
	@echo -e "    debug       - Build in debug mode (-g -O0)"
	@echo -e "    release     - Build in release mode (-O2)"
	@echo -e "    check       - Perform syntax check only"
	@echo ""
	@echo -e "  $(GREEN)Run targets:$(RESET)"
	@echo -e "    run         - Run with $(NP) processes"
	@echo -e "    run-np N=<n> - Run with N processes"
	@echo -e "    buildrun    - Build and run"
	@echo -e "    test        - Test with different process counts"
	@echo ""
	@echo -e "  $(GREEN)Debug targets:$(RESET)"
	@echo -e "    debug-run   - Run with GDB debugger"
	@echo -e "    memcheck    - Run with Valgrind memory checker"
	@echo ""
	@echo -e "  $(GREEN)Utility targets:$(RESET)"
	@echo -e "    clean       - Remove build artifacts"
	@echo -e "    distclean   - Deep clean (includes backup files)"
	@echo -e "    install-mpi - Install MPI development tools"
	@echo -e "    info        - Show project information"
	@echo -e "    help        - Show this help"
	@echo ""
	@echo -e "  $(GREEN)Variables:$(RESET)"
	@echo -e "    BUILD_MODE  - debug or release (current: $(BUILD_MODE))"
	@echo -e "    NP          - Number of processes (current: $(NP))"
	@echo -e "    HOSTFILE    - Path to MPI hostfile (current: $(HOSTFILE))"
	@echo -e "    MPIFLAGS    - Additional MPI flags (current: $(MPIFLAGS))"
	@echo ""
	@echo -e "  $(YELLOW)Examples:$(RESET)"
	@echo -e "    make debug                    # Build in debug mode"
	@echo -e "    make release                  # Build in release mode"
	@echo -e "    make run NP=8                 # Run with 8 processes"
	@echo -e "    make run HOSTFILE=hosts.txt   # Run with hostfile"
	@echo -e "    make memcheck                 # Memory check"