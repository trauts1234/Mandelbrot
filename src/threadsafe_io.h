#pragma once

#include <iostream>
#include <syncstream>
#include <fstream>
#include <string>
#include <sstream>

#ifndef NDEBUG
#define sync_dbg std::osyncstream(std::cerr)
#else
#define sync_dbg std::ostream(nullptr)
#endif

#define sync_cout std::osyncstream(std::cout)