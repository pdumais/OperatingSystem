#define USER_PROCESS_STUB_SIZE 0x200
#define USER_PROCESS_CODE_START (0x08000000+USER_PROCESS_STUB_SIZE)

struct ProcessStructureInfo
{
    uint64_t* pml4;
    uint64_t* pageTables;
};

struct UserProcessInfo
{
    char* entryPoint;
    char* metaPage;
    uint64_t entryParameter;
    uint64_t consoleSteal;
    struct ProcessStructureInfo psi;
    uint64_t lastProgramAddress;
};

void createUserProcess(struct UserProcessInfo*);
void addUserProcessSection(struct UserProcessInfo* upi, char* buffer, uint64_t virtualAddress, uint64_t size, bool readOnly, bool executable, bool initZero);
void launchUserProcess(struct UserProcessInfo* upi);
