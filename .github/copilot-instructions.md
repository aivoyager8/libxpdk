# Copilot Instructions for libxpdk

<!-- Use this file to provide workspace-specific custom instructions to Copilot. For more details, visit https://code.visualstudio.com/docs/copilot/copilot-customization#_use-a-githubcopilotinstructionsmd-file -->

## Project Overview
This is a C library project that wraps SPDK (Storage Performance Development Kit) bdev functionality with POSIX-like interfaces for block storage operations.

## Key Design Principles
- Provide simple, POSIX-like APIs to hide SPDK complexity
- Thread-safe operations using pthreads
- Error handling with proper return codes
- Memory management best practices
- C11 standard compliance
- No external dependencies except SPDK and standard C library

## Code Style Guidelines
- Use snake_case for function names and variables
- Use snake_case with uppercase for constants and macros
- Prefer explicit error handling over exceptions
- Use proper const-correctness
- Follow RAII principles where applicable in C
- Use static functions for internal APIs

## SPDK Integration Notes
- Always initialize SPDK environment properly
- Handle SPDK callbacks and event loops
- Manage SPDK bdev lifecycle correctly
- Consider SPDK threading model in design
