#pragma once
#include "Services.h"

class CMyRedis;

class CMemSink
{
	CServices* const m_pService;
	CMyRedis* m_pMemData;
public:
	CMemSink(CServices* pMemData);
	~CMemSink();
public:
	bool Init();
	bool HandMemDataReq(SERVICEINDEX uFromSerId,SERVICEINDEX nCsid,UINT uTypeID, void *pData, DATASIZE size);
	bool HandTimeMsg(TIMEERID nTimeId);
};
