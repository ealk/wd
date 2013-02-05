//
// wd - a window watch dog (sort of)
// 
// The Application scans all top-level windows, reads out their 
// captions's text, compares it to a string passed on start-up and if
// it matches sends a WM_CLOSE to its window.
//
// Optionally wd takes a command line to execute prior to the scanning
// starts and terminates itsself in the event of the termination of the
// application tsarted.
//
//
// This software is subject to the GPL (Gnu Public License) in the current 
// version.
//
// 2013/1/31, Ernst Albrecht Köstlin (alk) <a.koestlin@gmx.net>
//

#include "stdafx.h"

#define APP_VERSION_MAJOR 0
#define APP_VERSION_MINOR 1
#define APP_VERSION_MICRO 3
#define APP_NAME L"wd (http://github.com/ealk/wd)"

void usage(const WCHAR * plszAppName)
{
	fprintf(stderr, "%ls v%d.%d.%d\n", APP_NAME, APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_MICRO);
	fprintf(stderr, "%ls: <window caption to be monitored> <milli seconds between monitoring runs> [\"<command>\"]\n", plszAppName);
}

BOOL CALLBACK EnumerateWindowsCB(
	HWND hWnd, 
	LPARAM lParam
	)
{
	WCHAR * plszCaption = NULL;
	int iLengthCaption = 0;

	SetLastError(0); // set this explicitly as GetWindowTextLength might return 0 without error condition
	iLengthCaption = GetWindowTextLength(hWnd);
	if (!iLengthCaption)
	{
		DWORD dwLastError = GetLastError();
		if (dwLastError && ERROR_INVALID_WINDOW_HANDLE != dwLastError)
		{
			fprintf(stderr, "GetWindowTextLength(hWnd=0x%x, ...) failed with 0x%x.\n", hWnd, dwLastError);
			goto lblExit;
		}
	}

	plszCaption = malloc((iLengthCaption + 1) * sizeof (*plszCaption));

	{
		int iResult = GetWindowText(hWnd, plszCaption, iLengthCaption + 1);
		if (!iResult)
		{
			DWORD dwLastError = GetLastError();
			if (dwLastError && ERROR_INVALID_WINDOW_HANDLE != dwLastError)
			{
				fprintf(stderr, "GetWindowText(hWnd=0x%x, ...) failed with 0x%x.\n", hWnd, dwLastError);
				goto lblExit;
			}
		}
	}

#ifdef _DEBUG
	printf("0x%08x: '%ls'\n", hWnd, plszCaption);
#endif

	if (!wcscmp(plszCaption, (WCHAR *) lParam))
	{
		int iResult = PostMessage(hWnd, WM_CLOSE, 0, 0);
		if (!iResult)
		{
			DWORD dwLastError = GetLastError();
			if (dwLastError && ERROR_INVALID_WINDOW_HANDLE != dwLastError)
			{
			  fprintf(stderr, "PostMessage(hWnd=0x%x [caption='%ls'], ...) failed with 0x%x.\n", hWnd, plszCaption, dwLastError);
			}
		}
	}

lblExit:

	free(plszCaption);

	return 1;
}

int TestCaption(
	const WCHAR * plszCaption
	)
{
	int iResult = EXIT_SUCCESS;

	if (!EnumWindows(EnumerateWindowsCB, (LPARAM) plszCaption))
	{
		iResult = EXIT_FAILURE;
		fprintf(stderr, "EnumWindows() failed with 0x%x.\n", GetLastError());
		goto lblExit;
	}

lblExit:

	return iResult;
}

int SpawnProcess(
	const WCHAR * plszCommandLine, 
	HANDLE * phProcess
	)
{
	int iResult = EXIT_SUCCESS;

	PROCESS_INFORMATION pi;
    STARTUPINFO si;
    memset(&pi, 0, sizeof(pi));
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);

    if (plszCommandLine)
    {
        WCHAR lszCommandLine[(MAX_PATH + 1) * 2];  //Needed since CreateProcessW may change the contents of CmdLine

        wcscpy_s(lszCommandLine, MAX_PATH * 2, plszCommandLine);

#ifdef _DEBUG
    	printf("About spawn: '%ls'\n", lszCommandLine);
#endif

        if (!CreateProcess(NULL, lszCommandLine, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi))
		{
			fprintf(stderr, "CreateProcess() failed with 0x%x.\n", GetLastError());
			iResult = EXIT_FAILURE;
			goto lblExit;
		}

		*phProcess = pi.hProcess;
    }

lblExit:

	return iResult;
}

int ProcessRunning(
	const HANDLE hProcess, 
	BOOL * pbRunning
	)
{
	int iResult = EXIT_SUCCESS;

	*pbRunning = TRUE;

	{
		DWORD dwResult = 0;

		switch(dwResult = WaitForSingleObject(hProcess, 0))
		{
		case WAIT_OBJECT_0:
			*pbRunning = FALSE;
			break;

		case WAIT_TIMEOUT:
			break;

		case WAIT_FAILED:
			iResult = EXIT_FAILURE;
			fprintf(stderr, "WaitForSingleObject() failed with 0x%x.\n", GetLastError()); 
			break;

		case WAIT_ABANDONED:
		default:
			/* should not happen */
			iResult = EXIT_FAILURE;
			fprintf(stderr, "WaitForSingleObject() returned unexpected result with 0x%x.\n", dwResult); 
			break;
		}
	}

	return iResult;
}

int wmain(
	int iArgC, 
	WCHAR * pplszArgV[]
	)
{
	int iResult = EXIT_SUCCESS;

	if (3 > iArgC || 4 < iArgC)
	{
		iResult = EXIT_FAILURE;
		fprintf(stderr, "Invalid number of arguments.\n");
		usage(pplszArgV[0]);
		goto lblExit;
	}

	{
		HANDLE hProcess = 0;
	
		if (4 == iArgC)
		{
			if (iResult = SpawnProcess(pplszArgV[3], &hProcess))
			{
				fprintf(stderr, "Spawning '%ls' failed.\n",  pplszArgV[3]);
				goto lblExit;
			}
		}

		{
			DWORD dwMilliSeconds = (1 < iArgC) ?_wtol(pplszArgV[2]) :0;

			while (1)
			{
				if (hProcess)
				{
					BOOL bRunning = TRUE;

					if (iResult = ProcessRunning(hProcess, &bRunning))
					{
						fprintf(stderr, "ProcessRunning() failed.\n"); 
						break;
					}

					if (!bRunning)
					{
						break;
					}
				}

				if (iResult = TestCaption(pplszArgV[1]))
				{
					fprintf(stderr, "TestCaption() failed.\n");
					break;
				}

				Sleep(dwMilliSeconds);
			} 

			if (iResult)
			{
				goto lblExit;
			}
		}
	}

lblExit:

	return iResult;
}

// EOF