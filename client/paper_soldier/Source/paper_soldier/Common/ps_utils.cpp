// Fill out your copyright notice in the Description page of Project Settings.

#include "Engine.h"
#include "ps_utils.h"

// �׻� �ذ��ؾ��ϴ� ġ������ ������ ���� �α� 
DECLARE_LOG_CATEGORY_EXTERN(AssertError, Log, All);
DEFINE_LOG_CATEGORY(AssertError);

using namespace std;

void Shutdown(const TCHAR*_file, const int& _line, const TCHAR* _function)
{

#if WITH_EDITOR
  PrintLog(_file, _line, _function);
  QuitTheGame();
#endif

}

void PrintLog(const TCHAR*_file, const int& _line, const TCHAR*_function)
{
			 
  UE_LOG(AssertError, Error, 
		 TEXT("Assert [ File : %s Line : %d Function : %s]"),
			_file, _line, _function);

}

void QuitTheGame() {

  //check assert
  check(GWorld);

  APlayerController *player_controller = GWorld->GetFirstPlayerController();
  check(player_controller);

  //player_controller->IsLocalController();
  player_controller->ConsoleCommand("quit");

}