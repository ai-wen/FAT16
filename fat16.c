#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <ctype.h>

#include "fat16.h"
#include "sector.h"
#include "log.h"

#define LENGTH_BITS_SECONDS 5
#define LENGTH_BITS_MINUTES 6
#define LENGTH_BITS_HOURS 5
#define LENGTH_BITS_DAYS 5
#define LENGTH_BITS_MONTHS 4
#define LENGTH_BITS_YEARS 7

//-----------------------------------------------------------------------------
BPB bpb;

uint16_t RootDirSectors;
uint16_t FirstRootDirSecNum;
uint16_t FirstDataSector;
int NUM_DIR_PER_SECTOR;
//FILE *f;
uint16_t *fat;
extern const int LENGTH_SECTOR;

//------------------------------------------------------------------------------

//carrega a FAT na memoria

/*  parametros
    fd -> descritor do arquivo
*/
void init_fat(FILE *fd)
{


    //aloca a FAT
    fat = (uint16_t * )calloc( bpb.BPB_FATSz16 * (bpb.BPB_BytsPerSec/2) ,sizeof(uint16_t) );
    int i;

    //le a FAT
    for(i = 0; i < bpb.BPB_FATSz16 ; i++)
    {
        sector_read(fd , bpb.BPB_RsvdSecCnt + i , fat + (i*LENGTH_SECTOR/2));
    }
}

//verifica se um cluster é um final cluster
/*  parametros
    num_cluster -> cluster dado para verficação
*/
int is_final_cluster(uint16_t num_cluster)
{
    //verfica de acordo com a documentação da FAT
    if((num_cluster >= 0xFFF8 && num_cluster <= 0xFFFE) || (num_cluster == 0xFFFF) || (num_cluster < 2) )
        return 1;
    else
        return 0;

}

// "Decodifica" o dia de acordo com a data dada no formato da FAT
int get_day(uint16_t dir_date)
{
    int aux = (1 << LENGTH_BITS_DAYS) - 1;
    int  dir_day =  (dir_date & aux);
    return dir_day;
}

// "Decodifica" o mes de acordo com a data dada no formato da FAT
int get_month(uint16_t dir_date)
{
    int aux = (1 << LENGTH_BITS_MONTHS) - 1;
    int dir_month  = ((dir_date >> LENGTH_BITS_DAYS) & aux);
    return dir_month;
}

// "Decodifica" o ano de acordo com a data dada no formato da FAT
int get_year(uint16_t dir_date)
{
    int dir_year  = (dir_date >> (LENGTH_BITS_MONTHS + LENGTH_BITS_DAYS));
    return 1980+dir_year;
}

// "Decodifica" a hora de acordo com a "horas" dada no formato da FAT
int get_hour(uint16_t dir_time)
{
    int dir_hour = (dir_time >> (LENGTH_BITS_MINUTES + LENGTH_BITS_SECONDS));
    return dir_hour;
}


// "Decodifica" os minutos de acordo com a "horas" dada no formato da FA
int get_minutes(uint16_t dir_time)
{
    int aux = (1 << LENGTH_BITS_MINUTES) - 1;
    int dir_minute = ((dir_time >> LENGTH_BITS_SECONDS) & aux);
    return dir_minute;
}


// "Decodifica" os segundos de acordo com a "horas" dada no formato da FA
int get_seconds(uint16_t dir_time)
{
    int aux = (1 << LENGTH_BITS_SECONDS) - 1;
    int dir_seconds = (dir_time & aux);
    return dir_seconds;
}

//Dado um cluster retorna o primeiro setor desse cluster
/*  parametros

    num_cluster -> numero do cluster
*/
int get_first_sector_of_cluster(int num_cluster)
{
    //devemos pega o cluster - 2 de um cluster e multiplicá-lo pe quantidade de setores por cluster para encontrar qual setor queremos.
    //Coloca o deslocamento do primeiro setor de dados e assim encotramos o primeiro setor de um dado cluster
    uint16_t FirstSectorofCluster = ( (num_cluster - 2) * bpb.BPB_SecPerClus) + FirstDataSector;
    return FirstSectorofCluster;
}


//dado o numero de um cluster, o lê da FAT e "grava" no buffer

/*  parametros

    fd -> descritor do arquivo da imagem
    num_cluster -> numero do cluster a ser lido
    cluster_buffer -> buffer do cluster a ser lido
*/
void  read_cluster(FILE *fd, int num_cluster , char * cluster_buffer)
{
    int i = 0;

    //encontra o primeiro setor do cluster
    uint16_t first_sector_of_cluster = get_first_sector_of_cluster(num_cluster);

    //itera sobre a quantidade de setores por cluster
    for(i = 0; i < bpb.BPB_SecPerClus; i++)
    {
        //le os setores e os coloca no buffer
        sector_read(fd,first_sector_of_cluster + i, cluster_buffer + (i*LENGTH_SECTOR) );
    }
}


//Dado um path e o i-esimo nome que é requirido neste path, o recupera e coloca em "name".
/*  parametros

    path -> path completo do arquivo
    name -> local onde sera gravado o nome do i-ésimo nome do path
    ith -> i-ésimo path desejado
*/
/*
    retorna 1 se encontrei o i-ésimo nome em path e 0 caso contrário
*/
int get_file_name(const char *path, char *name, int ith)
{

    //conta até chegar no i-ésimo path
    int count = 0;

    int i = 0;
    int k = 0;

    //percorre todos os caracteres do path
    for(i = 0; i < strlen(path); i++)
    {

        //caso encontre um '/', significa que encontramos um novo começo de nome de arquivo.
        if(path[i] == '/')
        {

            //Logo encontrado '/' incrementamos o count para contar até chegar no i-ésimo path desejado.
            count++;

            //cheguei no i-esimo path
            if(count == ith)
            {
                k = i+1;
                int j = 0;

                //coloco em name o nome do i-ésimo arquivo
                while(k < strlen(path) && path[k] != '/' && path[k] != '\0')
                {
                    name[j] = path[k];
                    j++;
                    k++;
                }
                //colca 0 na ultima posição da string
                name[j] = 0;

                //retorna 1 pois encontrei o i-ésimo path
                return 1;
            }
        }
    }
    //retorna 0 pois percorri todo path e não econtrei o i-ésimo path
    return 0;
}

//Dado uma string a formata no formato da FAT
/*  parametros

    name -> nome do arquivo a ser formatado

*/
/*
    retorna o nome formatado no formato da FAT
*/
char * format_string(char *name)
{
    int i = 0;

    int j = 0;

    //aloca espço para o nome que ser formatado
    char * formatted_name = (char * )calloc(11, sizeof(char));


    for(i = 0; i < 11; i++)
    {
        formatted_name[i] = ' ';
    }

    i = 0;

    while(i < strlen(name) && i < 11)
    {
        //se o caracter atual for uma letra minuscula, coloca esse caracter é um letra maiuscula correspondente
        if(name[i] >= 'a' && name[i] <= 'z')
            formatted_name[i] = name[i] - 32;

        //se o caracter for um ponto
        else if(name[i] == '.')
        {
            //verifica se o o ponto não é o último caracter
            if(i+1 < strlen(name))
                i++;
            //caso seja, nada mais é necessario
            else
                break;

            //colca nas posições especificadas pela FAT a extensão do arquivo
            for(j = 8; i < strlen(name) && j < 11; i++, j++)
            {
                //coloca as letras na forma maiuscula
                if(name[i] >= 'a' && name[i] <= 'z')
                    formatted_name[j] = name[i] - 32;

                //caso nao seja um letra, somente o coloca em seu devido local
                else
                    formatted_name[j] = name[i];
            }
        }
        //caso nao seja um ponto e nao seja um letra somente o coloca em seu devido local
        else
            formatted_name[i] = name[i];
        i++;
    }

    //retorna o nome formatado
    return formatted_name;
}

//Dado um nome, verifica se o nome é um nome valido pela FAT.
/*  parametros
    name -> nome a ser verificado
*/
/*
    retorna 1 se o nome for valido e 0 caso contrário
*/
int is_valid_name(char *name)
{
    //verfica se o nome é valido pelas especificações da FAT
    if(name[0] == 0 || name[0] == 0xE5 || name[0] == 0xFFFFFFFFFFFFFFE5 )
        return 0;
    else if((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= '0' && name[0] <= '9') || (name[0] >= 127 && name[0] <= 255))
        return 1;
    else
        return 0;
}

//Dado um primeiro cluster e o ith cluster a partir do primeiro cluster retorna le as entradas de diretorio desse cluster e coloca em um buffer
/*  parametros

    fd -> descritor de arquivo da imagem
    first_cluster -> primeiro cluster do arquivo
    ith_cluster -> i-ésimo cluster a partir do primeiro cluster do arquivo
    dir_entries -> buffer para as entradas de diretorio.
*/
void read_dir_entries(FILE *fd , uint16_t first_cluster , int ith_cluster, DIR *dir_entries)
{
    int i;

    i = 0;

    //encontra o i-ésimo cluster a partir do primeiro cluster
    while(i < ith_cluster && !is_final_cluster(first_cluster))
    {
        i++;
        first_cluster = fat[first_cluster];
    }

    //éncontra o primeiro setor do cluster
    uint16_t first_sector_of_cluster = get_first_sector_of_cluster(first_cluster);

    // coloca a quantidade de setores por cluster no buffer, com deslocamento de numeros de diretorios por setor
    for(i = 0; i < bpb.BPB_SecPerClus; i++)
    {
        sector_read(fd , first_sector_of_cluster + i, &dir_entries[i*NUM_DIR_PER_SECTOR]);
    }

}

//Dado um nome no formato da FAT o coloca no formato do FUSE.
/*  parametros
    name -> nome a ser formatado
    buffer -> buffer onde sera colocado o nome formatado
*/
void name_for_fuse(char *name, char *buffer)
{
    //aloca espaço para o nome que sera formatado
    char * formatted_name = (char *) calloc(15, sizeof(char));

    int i = 0;

    int j = 0;

    //o valor 7 é artibuida de acordo com a especificação da FAT
    int limit = 7;

    //flag para verificar se foi encontrada uma letra
    int has_letter = 0;

    //encontra a ultima posiçao que nao seja um espaço
    while(name[limit] == ' ')
      limit--;


    for(i = 0; i <= limit; i++)
    {
      //coloca as letras em forma minuscula no nome formatado
      if(name[i] >= 'A' && name[i] <= 'Z')
          formatted_name[j++] = name[i] + 32;

      //caso nao seja uma letra nao altera sua forma, somenete coloca em formatted_name
      else
          formatted_name[j++] = name[i];
    }

    j = limit+1;

    //coloca o ponto no nome do arquivo para delimitar a extensao
    if(name[8] != ' ')
        formatted_name[j++] = '.';


    has_letter = 0;

    //parte da extensão do arquivo
    for(i = 8; i < 11; i++)
    {
        //colca as letra na forma minuscula
        if(name[i] >= 'A' && name[i] <= 'Z')
          formatted_name[j++] = name[i] + 32;

        //caso nao foi encontrado nenhuma letra na externsao e somente espaços nada mais é necessario
        else if(name[i] == ' ' && !has_letter)
          break;

        //coloca espaço e demais caracteres no nome formatado
        else
          formatted_name[j++] = name[i];
    }

    formatted_name[j] = '\0';

    //copia o nome formatado para o buffer
    strcpy(buffer,formatted_name);

    //libera o espaço de memoria do nome formatado
    free(formatted_name);
}

//Dado o path e o descritor de arquivo , encontra o arquivo e o grava em file

/*  parametros

    fd -> descritor de arquivo
    path -> path do arquivo
    file -> buffer onde sera armazenado a entrada de diretorio do arquivo do path
*/
int  path_to_dir_entry(FILE *fd, const char *path , DIR * file)
{
      //aloca o buffer das entradas de diretorio para cluster
      DIR  *dir_cluster = (DIR *) calloc((NUM_DIR_PER_SECTOR * bpb.BPB_SecPerClus), sizeof(DIR) );

      //aloca o buffer das entradas de diretorio para setor
      DIR  *dir_sector = (DIR *) calloc((NUM_DIR_PER_SECTOR), sizeof(DIR) );

      int i = 0;
      int j = 0;
      int k = 0;
      int found = 0;

      // numero de nomes no path
      int num_names = 0;

      //conta o numero de nomes no path
      for(i = 0; i < strlen(path); i++)
      {
          if(path[i] == '/')
              num_names++;
      }

    //aloca espaço para o nome do arquivo a ser encontrado
      char *name_to_be_found = calloc(sizeof(path),1);

    //encontra o nome do arquivo a ser enconrtado
      found = get_file_name(path, name_to_be_found,num_names);

    //se nao exister esse nome retorna 0
      if(!found)
        return 0;

      //formata o nome do arquivo a ser encontrado
      name_to_be_found = format_string(name_to_be_found);

      //nome atual durante a busca no diretorios da imagem
      char *current_name = calloc(sizeof(path),1);

      //pega o primeiro nome do path, ou seja o nome do arquivo/diretorio que está no root
      get_file_name(path,current_name,1);

      //formata o nome atual no formato da FAT
      current_name = format_string(current_name);

      //flag para verifcar se o nome a ser encontrado foi encontrado
      found = 0;

      //log_msg("num_names: [%d] e name_to_be_found: [%s] e path: [%s]\n",num_names,name_to_be_found,path);

      //busca ,do nome do arquivo a ser encontrado, no root
      for(j = 0; j < RootDirSectors; j++)
      {
          //"zera" o buffer de setor
          memset(dir_sector, 0 ,sizeof(dir_sector));

          //le o j-ésimo setor do root
          sector_read(fd , FirstRootDirSecNum + j , dir_sector);

          //"navega" nas entradas de diretorio do buffer
          for(k = 0; k < NUM_DIR_PER_SECTOR; k++)
          {
              // condiçao de parada
              if(dir_sector[k].DIR_Name[0] == 0)
              {
                  j = RootDirSectors;
                  break;
              }
              //log_msg("sector: %d nome atual no root:[%s], dir_cluster[%d] = %s path = %s\n",FirstRootDirSecNum + j,current_name,k,dir_sector[k].DIR_Name,path);

              //verifica se o nome atual é a k-ésima entrada de diretorio
              if(memcmp(current_name , dir_sector[k].DIR_Name , 11 ) == 0)
              {
                  //"seta" a a flag para 1;
                  found = 1;

                  //coloca no file o a k-ésima entrada de diretorio
                  *file = dir_sector[k];

                  //verifica se o nome atual é o nome a ser encontrado
                  if(memcmp(current_name , name_to_be_found, 11) == 0)
                  {
                      //se encontrei o nome a ser encontrado, libero os buffers e retorno 1
                      free(dir_sector);
                      free(dir_cluster);
                      return 1;
                  }
                  //nao preciso "olhar" o restante das entradas de diretorio
                  break;
              }
          }
      }
      //caso nao achei nenhum diretorio entre os setores do root retorno 0 e desaloco os buffers
      if(!found)
      {
          free(dir_sector);
          free(dir_cluster);
         // log_msg("retornei pq nao tem o path %s\n",path);
          return 0;
      }

      found = 0;
      //busca nos subdiretorios do root
      //iterato a partir do segundo nome do path
      for(i = 2; i <= num_names; i++)
      {
          //"zera" o nome atual
          memset(current_name, 0 , sizeof(current_name));

          //encontro o i-ésimo nome do path
          found  = get_file_name(path,current_name,i);
          if(!found)
              return 0;

          //formato no nome atuak
          current_name = format_string(current_name);

          //primeiro cluster do diretorio corrente durante a busca
          int f_cluster = (*file).DIR_FstClusLO;

          int ith = 0;

          found = 0;

          while(!is_final_cluster(f_cluster))
          {
              //"zera" o buffer de cluster
              memset(dir_cluster,0,sizeof(dir_cluster));

              //leio a i-esima entrada de diretorio a partir do primeiro cluster
              read_dir_entries(fd ,f_cluster, ith , dir_cluster);

              //itera sobre as entradas de diretorio do buffer do cluster
              for(k = 0; k < (NUM_DIR_PER_SECTOR * bpb.BPB_SecPerClus); k++)
              {
                  //confdiçao de parada
                  if(dir_cluster[k].DIR_Name[0] == 0)
                  {
                      //quebra o while
                      f_cluster = 0;
                      break;
                  }


                  //forma analoga a busca no root descrita acima
                  //verifica se a k-esima entrada de diretorio tem o mesmo nome do nome atual
                  if(memcmp(current_name , dir_cluster[k].DIR_Name , 11 ) == 0)
                  {
                      //forma analoga a busca no root
                      found = 1;
                      *file = dir_cluster[k];
                      if(i == num_names && (memcmp(current_name,name_to_be_found,11) == 0))
                      {
                          free(dir_sector);
                          free(dir_cluster);
                          return 1;
                      }
                      break;
                  }
              }
              //incremento o i-esimo cluster
              ith++;

              //vou para o proximo cluster
              f_cluster = fat[f_cluster];
          }
          //se nao encontrei libero os buffers e retorno 0
          if(!found)
          {
              free(dir_sector);
              free(dir_cluster);
              return 0;
          }
      }
      //libero os buffers
      free(dir_sector);
      free(dir_cluster);
      return 0;
}
