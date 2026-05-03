// Aes128Ctr.h
#ifndef AES128CTR_H
#define AES128CTR_H

void Aes128CtrCrypt(IN OUT unsigned char* pBuffer, IN unsigned __int64 uBufferSize, IN unsigned char* pAesKey, IN unsigned char* pAesIv, IN unsigned __int64 uFileOffset);

#endif 