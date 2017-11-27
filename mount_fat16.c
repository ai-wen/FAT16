#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "fat16.h"
#include "sector.h"
#include "log.h"

extern const int LENGTH_SECTOR;// tamanho do setor
extern BPB bpb; // BPB

FILE *f;// arquivo a ser aberto
extern uint16_t RootDirSectors;

extern uint16_t FirstRootDirSecNum;

extern uint16_t FirstDataSector;

extern int NUM_DIR_PER_SECTOR;

extern uint16_t *fat;//FILE ALLOCATION TABLE

//struct atributos de tempo
struct tm get_time(uint16_t _time, uint16_t _date)
{
  struct tm time_info;
  //converte os segundos do formato da FAT para o formato do UNIX
  time_info.tm_sec = get_seconds(_time);

  //converte os minutos do formato da FAT para o formato do UNIX
  time_info.tm_min = get_minutes(_time);

  //converte as horas do formato da FAT para o formato do UNIX
  time_info.tm_hour = get_hour(_time);

  //converte o dia do formato da FAT para o formato do UNIX
  time_info.tm_mday = get_day(_date);

  //converte o mes do formato da FAT para o formato do UNIX
  time_info.tm_mon = get_month(_date) - 1;

  //converte o ano do fomato da FAT para o formato do UNIX
  time_info.tm_year = get_year(_date) - 1900;

  return time_info;
}

//inicia a fat
void *fat16_init(struct fuse_conn_info *conn)
{
  sector_read(f, 0, &bpb); // le o BPB

  //carrega a FAT (file allocation table) para a memoria.
  init_fat(f);

  //numero de setores na raiz
  RootDirSectors = (bpb.BPB_RootEntCnt * 32) / bpb.BPB_BytsPerSec;

  //numero do primeiro setor da raiz
  FirstRootDirSecNum = bpb.BPB_RsvdSecCnt + (bpb.BPB_NumFATs * bpb.BPB_FATSz16);

  //numero de entradas de diretorio por setor
  NUM_DIR_PER_SECTOR = bpb.BPB_BytsPerSec/(int)(sizeof(DIR));

  //primeiro setor de dados
  FirstDataSector = bpb.BPB_RsvdSecCnt + (bpb.BPB_NumFATs * bpb.BPB_FATSz16) +  RootDirSectors;
}

//open dir na fat
 int fat16_opendir(const char *path, struct fuse_file_info *fi)
{
  //se o path for "/", retorna 0
  if(strcmp(path,"/") == 0)
  {
    return 0;
  }

  DIR dir = {0};
  int found = path_to_dir_entry(f,path, &dir); //found recebe a função path_to_dir_entry

  //se found não existe ou o nome do diretorio não é valido retorna -ENOENT
  if(!found || !is_valid_name(dir.DIR_Name))
    return -ENOENT;
  //se atributos do diretorio sao diferentes retorna -ENOENT
  if(dir.DIR_Attr != ATTR_DIRECTORY)
    return -ENOENT;
  else
    return 0;
}

//libera diretorio
int fat16_releasedir(const char *path, struct fuse_file_info *fi)
{

  //se o path for "/", retorna 0
  if(strcmp(path,"/") == 0)
  {
    return 0;
  }

  DIR dir = {0};
  int found = path_to_dir_entry(f,path,&dir); //found recebe a função path_to_dir_entry

  //se found não existe ou o nome do diretorio não é valido retorna -ENOENT
  if(!found || !is_valid_name(dir.DIR_Name))
    return -ENOENT;

  //se atributos do diretorio sao diferentes retorna -ENOENT
  if(dir.DIR_Attr == ATTR_ARCHIVE)
    return -ENOENT;

  return 0;
}


 int fat16_open(const char *path, struct fuse_file_info *fi)
{
  //nao foi implementada pois nao foi utilizado file handler
  return 0;
}

//libera fat
 int fat16_release(const char *path, struct fuse_file_info *fi)
{
  //nao foi implementada pois nao foi utilizado file handler
  return 0;
}

//le o arquivo
 int fat16_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    if(strcmp(path,"/") == 0)
    {
      return 0;
    }
    DIR file = {0};

    int found = path_to_dir_entry(f,path,&file);
    //verifica se encontrei  diretorio na fat
    if(!found)
      return -ENOENT;

    //verifica se o offset é maior que o tamanho do arquivo
    if(offset >= file.DIR_FileSize )
       return 0;

    //tamanho do cluster
    int len_cluster = LENGTH_SECTOR * bpb.BPB_SecPerClus;

    //variavel para andar na fat
    uint16_t num_cluster = file.DIR_FstClusLO;

    int i = 0;

    //buffer do cluster
    char * buffer_cluster = calloc(len_cluster, sizeof(char));

    //quantidade que eh necessario ler
    int size_to_read = 0;

    //"seta" a quantidade que eh necessario ler
    if((file.DIR_FileSize - offset) < size)
      size_to_read = file.DIR_FileSize - offset;
    else
      size_to_read = size;

    //cluster que tem a primeira parte do arquivo
    int pos_cluster = offset / len_cluster;

    //offset dentro cluster
    int offset_cluster = offset % len_cluster;

    //quantidade para ler no começo do arquivo
    int size_to_read_begin = len_cluster - offset_cluster;

    //quantidade que li
    int count = 0;

    //verifica se a quantidade necessaria para ler eh maior que o arquivo
    if(size_to_read > file.DIR_FileSize)
      return 0;

    //encontra o primeiro cluster a ser lido
    while(i < pos_cluster)
    {
        i++;
        num_cluster = fat[num_cluster];
    }

    //verifica se eh um cluster de fim de arquivo
    if(is_final_cluster(num_cluster))
        return 0;

    //verifica se a quantidade necessaria de ler no começo e menor que a quantidade total
    if(size_to_read_begin <= size_to_read)
    {
        //le o cluster
        read_cluster(f,num_cluster, buffer_cluster);

        //copia o pedaço do cluster para o buffer
        memcpy(buf , buffer_cluster + offset_cluster , size_to_read_begin);

        //incrementa a quantidade que foi lida
        count += size_to_read_begin;

        //vai para o proximo cluster
        num_cluster = fat[num_cluster];

        //diminui a quantidade que precisa ler ainda
        size_to_read -= size_to_read_begin;
    }

    if(is_final_cluster(num_cluster))
        return count;

    //le os cluster no meio do arquivo
    while(!is_final_cluster(num_cluster) && size_to_read >= len_cluster)
    {
        //le o cluster
        read_cluster(f,num_cluster, buffer_cluster);

        //copia o cluster para o buffer
        memcpy(buf+count, buffer_cluster,len_cluster);

        //decrementa a qunatidade necessaria para ler
        size_to_read -= len_cluster;

        //incrementa a quantidade que li
        count += len_cluster;

        //vai para o proximo cluster
        num_cluster = fat[num_cluster];
    }

    //verifica se o cluster é de fim de arquivo
    if(is_final_cluster(num_cluster))
        return count;

    //le o restante que faltou, que eh menor que um cluster
    if(size_to_read > 0 )
    {
        //le o cluster
        read_cluster(f,num_cluster, buffer_cluster);

        //copia so o restante do arquivo menor que um cluster
        memcpy(buf + count , buffer_cluster , size_to_read);

        //incrementa a quantidade que foi lida
        count += size_to_read ;
    }
    //libera o buffer
    free(buffer_cluster);

    return count;
}

//pega atributos
 int fat16_getattr(const char *path, struct stat *stbuf)
{
    //"zera" toda a struct stbuf;
    memset(stbuf,0,sizeof(struct stat *));

    //se o path for "/", retorna 0

    if(strcmp(path,"/") == 0)
    {
      //"seta" a struct do raiz
      stbuf->st_mode = S_IFDIR;
      stbuf->st_nlink = 2;
      stbuf->st_blksize = bpb.BPB_SecPerClus * LENGTH_SECTOR;
      stbuf->st_uid = getuid();
      stbuf->st_gid = getgid();
      stbuf->st_blocks = RootDirSectors/stbuf->st_blksize;

      return 0;
    }
    else
    {
      DIR dir = {0};
      int found = path_to_dir_entry(f,path,&dir); //found chama função path_to_dir_entry

      //se found nao existe retorna -ENOENT
      if(!found )
      {
        //log_msg("getattr: nao achei o %s porque nao tem na fat16\n",path);
        return -ENOENT;
      }

      //se o nome não é valido retorna -ENOENT
      if(!is_valid_name(dir.DIR_Name))
      {
       //log_msg("getattr: nao achei o %s porque eh um nome invalido\n",path);
        return -ENOENT;
      }
      //atributos para arquivo
      if(dir.DIR_Attr == ATTR_ARCHIVE)
      {
        stbuf->st_nlink = 1;
        stbuf->st_size = dir.DIR_FileSize;
        stbuf->st_mode =  S_IFREG | 0777 ;
      }
      //atributos para diretorio
      else if(dir.DIR_Attr == ATTR_DIRECTORY)
      {
        stbuf->st_size = 0;
        stbuf->st_nlink = 2;
        stbuf->st_mode = S_IFDIR | 0777;
      }

      //struct dos atributos em comum entre ambos
      struct tm time_info = {0};

      time_info = get_time(dir.DIR_WrtTime,dir.DIR_WrtDate);
      stbuf->st_mtime = mktime(&time_info);
      time_info = get_time(dir.DIR_CrtTime,dir.DIR_CrtDate);
      stbuf->st_atime = mktime(&time_info);
      stbuf->st_blksize = bpb.BPB_SecPerClus * LENGTH_SECTOR;
      stbuf->st_uid = getuid();
      stbuf->st_gid = getgid();
      stbuf->st_blocks = dir.DIR_FileSize/stbuf->st_blksize;
    }

    return 0;
}

//le diretorio fat
int fat16_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi)
{
    int j,k;
    //buffer das entradas de diretorios dos subdiretorios do root
    DIR *dir_cluster = (DIR *) calloc(NUM_DIR_PER_SECTOR * bpb.BPB_SecPerClus, sizeof(DIR));

    //buffer das entradas de diretorios do root
    DIR *dir_sector = (DIR *) calloc(NUM_DIR_PER_SECTOR , sizeof(DIR));

    //nome do arquivo
    char *name = (char *) calloc(strlen(path), 1);

    //trata o caso se o path ser o root
    if(strcmp(path,"/") == 0)
    {
        //como o root nao tem os diretorios "." e ".." devemos "criá-los"
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);

        //navega nos setores da root
        for(j = 0; j < RootDirSectors; j++)
        {
            //"zera" o buffer dir_sector para leitura no mesmo
            memset(dir_sector,0,sizeof(dir_sector));

            //le o j-ésimo setor do root
            sector_read(f , FirstRootDirSecNum + j,dir_sector);

            //procura nas entradas de diretorio do j-ésimo setor do root
            for(k = 0; k < NUM_DIR_PER_SECTOR; k++)
            {
                //condiçao de parada do procedimento
                if(dir_sector[k].DIR_Name[0] == 0)
                {
                    //utilizado para "invalidar" o for da linha 341
                    j = RootDirSectors;
                    break;
                }

                // verifica se é um nome válido
                if(is_valid_name(dir_sector[k].DIR_Name))
                {
                    //converte o nome no formato fat para o do FUSE
                    name_for_fuse(dir_sector[k].DIR_Name,name);

                    //coloca o nome do arquivo formatado na diretorio de montagem da imagem
                    filler(buf,name,NULL,0);
                }
            }
        }
    }
    else
    {
        DIR dir_entry = {0};
        //carrega  a entrada de diretorio de acordo com o path e armazena em dir_entry
        int found = path_to_dir_entry(f,path, &dir_entry);

        //caso nao encontrei o diretorio retorna -ENOENT
        if(!found )
        {
            //log_msg("readdir: nao achei o %s porque nao tem na fat16\n",path);
            return -ENOENT;
        }

        //se o nome encontrado for um nome invalido retorna -ENOENT
        if(!is_valid_name(dir_entry.DIR_Name))
        {
            //log_msg("readdir: nao achei o %s porque eh um nome invalido\n",path);
            return -ENOENT;
        }
       // log_msg("\n\n\nreaddir: achei o %s\nName: %s\nsize: %d\nfirst_cluster: %d\n\n\n",path,normalize_name(dir1.DIR_Name),dir1.DIR_FileSize,dir1.DIR_FstClusLO);

        //primeiro cluster do arquivo
        int f_cluster = dir_entry.DIR_FstClusLO;

        //i-ésimo cluster a partir do primeiro cluster
        int ith = 0;

        //procura ate nao encontra um "final cluster"
        while(!is_final_cluster(f_cluster))
        {
            //de modo analogo ao root tambem "zera" o buffer
            memset(dir_cluster,0,sizeof(dir_cluster));

            //le as entradas de diretorio de acordo com o i-ésimo cluster
            read_dir_entries(f ,f_cluster, ith , dir_cluster);

            //itera sobre todas as entradas de diretorio do buffer
            for(k = 0; k < NUM_DIR_PER_SECTOR * bpb.BPB_SecPerClus; k++)
            {
                //condiçao de parada
                if(dir_cluster[k].DIR_Name[0] == 0)
                {
                    f_cluster = 0;
                    break;
                }

                //se é um nome valido posso convertê-lo
                if(is_valid_name(dir_cluster[k].DIR_Name))
                {
                    name_for_fuse(dir_cluster[k].DIR_Name,name);
                    filler(buf,name,NULL,0);
                }
            }
            //incremento o i-esimo cluster
            ith++;

            //vai para o proximo cluster
            f_cluster = fat[f_cluster];
        }
    }

    //libera os buffers e o name
    free(dir_cluster);
    free(dir_sector);
    free(name);
    return 0;
}


//finaliza fat
void fat16_destroy(void *data)
{
  free(data); //libera data
  fclose(f);  //fecha f
}

//------------------------------------------------------------------------------
//operaçoes do FUSE
struct fuse_operations fat16_oper = {
  .init       = fat16_init,
  .destroy    = fat16_destroy,
  .getattr    = fat16_getattr,
  .opendir    = fat16_opendir,
  .readdir    = fat16_readdir,
  .open       = fat16_open,
  .releasedir = fat16_releasedir,
  .release    = fat16_release,
  .read       = fat16_read,
};


//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
  int ret;

  log_open(); //inicia o log

  f = fopen("/home/kataki/Documentos/so2-group-02/src/fat16.img","r"); //abre o arquivo
  
  //se f não é nulo chama a fuse, se não mostra msg de erro
  if(f != NULL)
    ret = fuse_main(argc, argv, &fat16_oper, NULL);
  else
    log_msg("Falha ao abrir o arquivo, verifique o path no fopen\n");

  return ret; //retorna a chamada do fuse
}
