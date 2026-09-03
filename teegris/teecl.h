#define TEEC_VALUE_INPUT 1
#define TEEC_VALUE_OUTPUT 2

void TEEC_AllocateSharedMemory(void**, void*);
void TEEC_CloseSession(void**);
void TEEC_FinalizeContext(void**);
void TEEC_InitializeContext(void*, void**);
void TEEC_InvokeCommand(void**, int, void*, int*);
void TEEC_RegisterSharedMemory(void**, void*);
void TEEC_ReleaseSharedMemory(void*);
void TEECS_OpenSession(void**, void**, const char*, void*, size_t, int, void*, void*, int*);
