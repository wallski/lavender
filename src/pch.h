#pragma once

#define WIN32_LEAN_AND_MEAN 
#define NOMINMAX


#include <Windows.h>
#include <Psapi.h>


#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <map>
#include <unordered_map>
#include <algorithm>


#include <d3d11.h>
#include <dxgi.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")