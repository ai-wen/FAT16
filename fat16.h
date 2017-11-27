
typedef enum {
    ATTR_DIRECTORY = 0x10, //verificar se eh diretorio
    ATTR_ARCHIVE = 0x20, // verificar se eh arquivo
}ATTR;

typedef struct BPB
{
  uint8_t BS_jmpBoot[3];
  unsigned char BS_OEMName[8];//identificador de nome 
  uint16_t BPB_BytsPerSec;//quantidade de bytes por setor
  uint8_t BPB_SecPerClus;//quantidade de setores por cluster
  uint16_t BPB_RsvdSecCnt;//quantidade de setores na regiao reservada
  uint8_t BPB_NumFATs;//numero de FATs
  uint16_t BPB_RootEntCnt;//quantidade de entradas de diretorio na raiz
  uint16_t BPB_TotSec16;//numero total de setores na fat
  uint8_t BPB_Media;
  uint16_t BPB_FATSz16;//conta o numero de setores ocupos por uma fat
  uint16_t BPB_SecPerTrk;//setores por track
  uint16_t BPB_NumHeads;
  uint32_t BPB_HiddSec; 
  uint32_t BPB_TotSec32;
  uint8_t BS_DrvNum;
  uint8_t BS_Reserved1;
  uint8_t BS_BootSig;
  uint32_t BS_VolID;
  unsigned char BS_VolLab[11];
  unsigned char BS_FilSysType[8];
  uint8_t offs[448];
  uint16_t Signature_word;

}__attribute__((packed)) BPB; // struct no bpb

typedef struct DIR
{
  char DIR_Name[11];//nome do diretorio
  uint8_t DIR_Attr;//atributo do diretorio
  uint8_t DIR_NTRes;
  uint8_t DIR_CrtTimeTenth;
  uint16_t DIR_CrtTime;//hora de criação
  uint16_t DIR_CrtDate;//data de criação
  uint16_t DIR_LstAccDate;//data de ultimo acesso
  uint16_t DIR_FstClusHI;
  uint16_t DIR_WrtTime;//hora de escrita
  uint16_t DIR_WrtDate;//data de escrita
  uint16_t DIR_FstClusLO;//primeiro cluster
  uint32_t DIR_FileSize; //tamanho do arquivo

}__attribute__((packed)) DIR;

// funções da FAT
int get_day(uint16_t dir_date);

int get_month(uint16_t dir_date);

int get_year(uint16_t dir_date);

int get_hour(uint16_t dir_time);

int get_minutes(uint16_t dir_time);

int get_seconds(uint16_t dir_time);

int get_first_sector_of_cluster(int num_cluster);

void read_cluster(FILE *fd,int num_cluster, char *cluster_buffer);

void read_dir_entries(FILE *fd , uint16_t first_cluster , int ith_cluster,DIR *dir1);

int is_final_cluster(uint16_t num_cluster);

void init_fat(FILE *fd);

void name_for_fuse(char *s, char *buffer);

int is_valid_name(char *s);

int  path_to_dir_entry(FILE *fd, const char *s ,DIR * file);
