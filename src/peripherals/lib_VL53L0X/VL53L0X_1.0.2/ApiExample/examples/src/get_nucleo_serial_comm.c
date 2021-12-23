#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "get_nucleo_serial_comm.h"
 
DWORD QueryKey(HKEY hKey, TCHAR *pSearchDevice, TCHAR *pPortName) 
{ 
    TCHAR    achKey[MAX_KEY_LENGTH];   // buffer for subkey name
    DWORD    cbName;                   // size of name string 
    TCHAR    achClass[MAX_PATH] = TEXT("");  // buffer for class name 
    DWORD    cchClassName = MAX_PATH;  // size of class string 
    DWORD    cSubKeys=0;               // number of subkeys 
    DWORD    cbMaxSubKey;              // longest subkey size 
    DWORD    cchMaxClass;              // longest class string 
    DWORD    cValues;              // number of values for key 
    DWORD    cchMaxValue;          // longest value name 
    DWORD    cbMaxValueData;       // longest value data 
    DWORD    cbSecurityDescriptor; // size of security descriptor 
    FILETIME ftLastWriteTime;      // last write time 
 
    DWORD i, retCode; 
 
    TCHAR  achValue[MAX_VALUE_NAME]; 
    DWORD cchValue = MAX_VALUE_NAME; 
    TCHAR  achDataValue[MAX_VALUE_NAME]; 
    DWORD cchDataValue = MAX_VALUE_NAME; 
 
    // Get the class name and the value count. 
    retCode = RegQueryInfoKey(
        hKey,                    // key handle 
        achClass,                // buffer for class name 
        &cchClassName,           // size of class string 
        NULL,                    // reserved 
        &cSubKeys,               // number of subkeys 
        &cbMaxSubKey,            // longest subkey size 
        &cchMaxClass,            // longest class string 
        &cValues,                // number of values for this key 
        &cchMaxValue,            // longest value name 
        &cbMaxValueData,         // longest value data 
        &cbSecurityDescriptor,   // security descriptor 
        &ftLastWriteTime);       // last write time 
 
    // Enumerate the subkeys, until RegEnumKeyEx fails.
    
    if (cSubKeys)
    {
        _tprintf(TEXT( "\nNumber of subkeys: %d\n"), (int)cSubKeys);

        for (i=0; i<cSubKeys; i++) 
        { 
            cbName = MAX_KEY_LENGTH;
            retCode = RegEnumKeyEx(hKey, i,
                     achKey, 
                     &cbName, 
                     NULL, 
                     NULL, 
                     NULL, 
                     &ftLastWriteTime); 
            if (retCode == ERROR_SUCCESS) 
            {
                _tprintf(TEXT("(%d) %s\n"), (int)(i+1), achKey);
            }
        }
    } 
 
    // Enumerate the key values. 

    if (cValues) 
    {
        printf( "\nNumber of values: %d\n", (int)cValues);

        for (i=0, retCode=ERROR_SUCCESS; i<cValues; i++) 
        { 
            cchValue = MAX_VALUE_NAME; 
            achValue[0] = '\0'; 
	    cchDataValue = MAX_VALUE_NAME;
            achDataValue[0] = '\0'; 
            retCode = RegEnumValue(hKey, i, 
                achValue, 
                &cchValue, 
                NULL, 
                NULL,
                achDataValue, 
                &cchDataValue );
            if (retCode == ERROR_SUCCESS) 
            { 	 
		_tprintf(TEXT("(%d) %s data %s  cchDataValue=%d\n"), (int)(i+1), achValue, achDataValue, (int)cchDataValue);
		if (strcmp(pSearchDevice, achValue) == 0) {
			strcpy(pPortName, achDataValue);
			_tprintf(TEXT("Found\n"));
			return 1;
		}
            } else {
                _tprintf(TEXT("ERROR (%d) %d \n"), (int)(i+1), (int)retCode);
	    }
        }
    }
    
    return 0;
}


DWORD GetNucleoSerialComm(TCHAR *pSerialCommStr)
{

   HKEY hTestKey;
   DWORD state = 0;
    TCHAR  achDataValue[MAX_VALUE_NAME]; 

   if( RegOpenKeyEx( HKEY_LOCAL_MACHINE,
        TEXT("HARDWARE\\DEVICEMAP\\SERIALCOMM"),
        0,
        KEY_READ,
        &hTestKey) == ERROR_SUCCESS
      )
   {
	state = QueryKey(hTestKey, "\\Device\\USBSER000", achDataValue);
	if (state == 1) {
		_tprintf(TEXT("found USBSER000 at port com %s\n"), achDataValue);
		strcpy(pSerialCommStr, achDataValue);		
	} else
		_tprintf(TEXT("ERROR USBSER000 not found!\n"));

   } 
   
   RegCloseKey(hTestKey);

   if (state == 1) 
	return 1;		
   else
	return 0;		

}


/*
void main(void)
{
   DWORD state = 0;
    TCHAR  SerialCommStr[MAX_VALUE_NAME]; 

   state = GetNucleoSerialComm(SerialCommStr);
   
   _tprintf(TEXT("%s\n"), SerialCommStr);
   
}

*/