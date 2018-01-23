// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


void Shutdown(const TCHAR* _file,  const int& _line, const TCHAR* _function);
void PrintLog(const TCHAR* _file, const int& _line, const TCHAR*_function);
void QuitTheGame();


#define PS_ASSERT(condition)\
  if (!condition) {\
	Shutdown(UTF8_TO_TCHAR(__FILE__), __LINE__, UTF8_TO_TCHAR(__func__));\
  }


