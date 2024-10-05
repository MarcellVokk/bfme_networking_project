#pragma once

#include "Renderer.h"

extern class Renderer g_pRenderer;

void SetGameHooks();
std::string HandleMessage(std::string message);