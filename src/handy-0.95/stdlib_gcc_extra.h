#ifdef ANSI_GCC 
#include <stdio.h> 
#include <unistd.h> 
#include <stdlib.h> 
#include <string.h> 
#include <errno.h> 


// Abstract:   split a path into its parts 
// Parameters: Path: Object to split 
//             Drive: Logical drive , only for compatibility , not considered 
//             Directory: Directory part of path 
//             Filename: File part of path 
//             Extension: Extension part of path (includes the leading point) 
// Returns:    Directory Filename and Extension are changed 
// Comment:    Note that the concept of an extension is not available in Linux, 
//             nevertheless it is considered 
void _splitpath(const char* Path,char* Drive,char* Directory,char* Filename,char* Extension) 
{ 
  char* CopyOfPath = (char*) Path; 
  int Counter = 0; 
  int Last = 0; 
  int Rest = 0; 

  // no drives available in linux .
  // extensions are not common in linux
  // but considered anyway
  // NOTE: this used to assign the local pointer (Drive = NULL), which left the
  // caller's buffer uninitialized. Callers strcpy() from it, so return "".
  if(Drive != NULL) *Drive = '\0';

  while(*CopyOfPath != '\0') 
    { 
      // search for the last slash 
      while(*CopyOfPath != '/' && *CopyOfPath != '\0') 
        { 
          CopyOfPath++; 
          Counter++; 
        } 
      if(*CopyOfPath == '/') 
        { 
          CopyOfPath++; 
         Counter++; 
          Last = Counter; 
        } 
      else 
          Rest = Counter - Last; 
    } 
  // directory is the first part of the path until the
  // last slash appears
  if(Directory != NULL)
  {
    strncpy(Directory,Path,Last);
    // strncpy doesnt add a '\0'
    Directory[Last] = '\0';
  }
  // Filename is the part behind the last slahs
  CopyOfPath -= Rest;
  if(Filename == NULL) return;
  strcpy(Filename,CopyOfPath);
  // get extension if there is any
  while(*Filename != '\0' && Extension != NULL)
  {
    // the part behind the point is called extension in windows systems 
    // at least that is what i thought apperantly the '.' is used as part 
    // of the extension too . 
    if(*Filename == '.') 
      { 
        while(*Filename != '\0') 
        { 
          *Extension = *Filename; 
          Extension++; 
          Filename++; 
        } 
      } 
      if(*Filename != '\0')
        {Filename++;}
  }
  if(Extension != NULL) *Extension = '\0';
  return;
}


// Abstract:   Make a path out of its parts 
// Parameters: Path: Object to be made 
//             Drive: Logical drive , only for compatibility , not considered 
//             Directory: Directory part of path 
//             Filename: File part of path 
//             Extension: Extension part of path (includes the leading point) 
// Returns:    Path is changed 
// Comment:    Note that the concept of an extension is not available in Linux, 
//             nevertheless it is considered 
void _makepath(char* Path,const char* Drive,const char* Directory,const char* File,const char* Extension) 
{   
  while(*Drive != '\0' && Drive != NULL) 
  { 
    *Path = *Drive; 
    Path++; 
    Drive++; 
  } 
  while(*Directory != '\0' && Directory != NULL) 
  { 
    *Path = *Directory; 
    Path ++; 
    Directory ++; 
  } 
  while(*File != '\0' && File != NULL) 
  { 
    *Path = *File; 
    Path ++; 
    File ++; 
  } 
  while(*Extension != '\0' && Extension != NULL) 
  { 
    *Path = *Extension; 
    Path ++; 
    Extension ++; 
  } 
  *Path = '\0'; 
  return; 
} 


// Abstract:   Change the current working directory 
// Parameters: Directory: The Directory which should be the working directory. 
// Returns:    0 for success , other for error 
// Comment:    The command doesnt fork() , thus the directory is changed for 
//             The actual process and not for a forked one which would be true 
//             for system("cd DIR"); 

int _chdir(const char* Directory)
{
  return (chdir(Directory)==0)?0:-1;
}


#endif
