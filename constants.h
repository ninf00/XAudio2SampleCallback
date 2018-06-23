#ifndef  _H_CONSTANTS
#define  _H_CONSTANTS

//-------------------------------------------------------------------------------------------------
// Helper macros
//-------------------------------------------------------------------------------------------------
#ifndef SAFE_DELETE
#define SAFE_DELETE(p)       { if (p) { delete (p);     (p)=NULL; } }
#endif  
#ifndef SAVE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p); (p)=NULL; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }
#endif

#define E_INVALIDARG                     _HRESULT_TYPEDEF_(0x80070057L)

#endif // ! _H_CONSTANTS

