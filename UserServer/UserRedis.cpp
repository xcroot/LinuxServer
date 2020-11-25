#include "UserRedis.h"
#include "UserServer.h"
#include "../MemDataBaseEnginer/MemDataDef.h"

extern CUserServer* g_pUserServer;

using namespace Mem;

CUserRedis::CUserRedis()
{
	for (int i = 0; i < PRO_MAX; i++)
	{
		m_RedisPro[i] = NULL;
	}
}

CUserRedis::~CUserRedis()
{
	
}

void CUserRedis::RegRedisScript(int nType,const char* szStr)
{
	redisReply* reply = (redisReply*)redisCommand(m_pConn, "SCRIPT LOAD %s", szStr);
	m_RedisPro[nType] = new char[reply->len+1];
	memcpy(m_RedisPro[nType], reply->str, reply->len);
	m_RedisPro[nType][reply->len] = 0;
	freeReplyObject(reply);
}

void CUserRedis::UserLoginReq(int nUserId,USHORT nSerNo,SERVICEINDEX nSevNo,void* pRet,UINT nRet)
{
	redisReply* pReply = (redisReply*)redisCommand(m_pConn, "EVALSHA %s %d %d %d %d", m_RedisPro[PRO_LOGIN_IN],3,nUserId,nSerNo,nSevNo);

	if(pReply == NULL)
	{
		CloseConnect();
		if(OpenConnect())
		{
			pReply = (redisReply*)redisCommand(m_pConn, "EVALSHA %s %d %d %d %d", m_RedisPro[PRO_LOGIN_IN],3,nUserId,nSerNo,nSevNo);
		}
		if(pReply == NULL)
			return;
	}

	if(pReply->type == REDIS_REPLY_ERROR)
	{
		printf("UserLoginReq RedisError %s\n",pReply->str);
		freeReplyObject(pReply);
		return;
	}
	
	UserLoginMemRet* pData = (UserLoginMemRet*)pRet;
	pData->nCid = pReply->element[0]->integer;
	pData->nCSid = pReply->element[1]->integer;
	pData->nGid = pReply->element[2]->integer;
	pData->nGSid = pReply->element[3]->integer;
	pData->nGsno = pReply->element[4]->integer;

	freeReplyObject(pReply);
}

void CUserRedis::UserJoinGame(int nUserId,USHORT nGid,SERVICEINDEX nGsid,int nSeatNo,void* pRet,UINT nRet)
{
	Mem::UserJoinGameRet* pData = (Mem::UserJoinGameRet*)pRet;
	redisReply* reply = (redisReply*)redisCommand(m_pConn, "EVALSHA %s %d %d %d %d %d",m_RedisPro[PRO_JOIN_GAME],4,nUserId,nGid,nGsid,nSeatNo);
	if(reply == NULL)
	{
		CloseConnect();
		if(OpenConnect())
		{
			reply = (redisReply*)redisCommand(m_pConn, "EVALSHA %s %d %d %d %d %d",m_RedisPro[PRO_JOIN_GAME],4,nUserId,nGid,nGsid,nSeatNo);
		}
		if(reply == NULL)
			return;
	}

	if(reply->type == REDIS_REPLY_ERROR)
	{
		printf("UserJoinGame RedisError %s\n",reply->str);
		return;
	}
	pData->nSeatNo = nSeatNo;
	pData->nCid = reply->element[0]->integer;
	pData->nCsid = reply->element[1]->integer;
	freeReplyObject(reply);
}


int  CUserRedis::GetRedisConfig()
{
	m_dbConfig.szHost = g_pUserServer->GetRedisHost();
	m_dbConfig.nPort = g_pUserServer->GetRedisPort();
	m_dbConfig.szAuth = g_pUserServer->GetRedisAuth();
	return 0;
}

bool CUserRedis::Connected()
{
	const char* szPro = "local uid = tonumber(KEYS[1]) \
		local cid = tonumber(KEYS[2]) \
		local csid = tonumber(KEYS[3]) \
		local ret = redis.call('HMGET',uid,'cid','csid','gid','gsid','gsno') \
		redis.call('HMSET',uid,'cid',cid,'csid',csid) \
		return {tonumber(ret[1] or 0),tonumber(ret[2] or 0),tonumber(ret[3] or 0),tonumber(ret[4] or 0),tonumber(ret[5] or 0)}";
	RegRedisScript(PRO_LOGIN_IN,szPro);

	szPro = "local uid = tonumber(KEYS[1]) \
		local cid = tonumber(KEYS[2]) \
		local csid = tonumber(KEYS[3]) \
		local ret = redis.call('HMGET',uid,'cid','csid','gid') \
		if tonumber(ret[1] or 0) == cid and tonumber(ret[2] or 0) == csid then \
			redis.call('HMSET',uid,'cid',0,'csid',0) \
			if tonumber(ret[3] or 0) == 0 then \
				redis.call('DEL',uid) \
			end \
		end";
	RegRedisScript(PRO_LOGIN_OUT,szPro);
	
	szPro = "local uid = tonumber(KEYS[1]) \
		local gid = tonumber(KEYS[2]) \
		local gsid = tonumber(KEYS[3]) \
		local gsno = tonumber(KEYS[4]) \
		redis.call('HMSET',uid,'gid',gid,'gsid',gsid,'gsno',gsno) \
		local ret = redis.call('HMGET',uid,'cid','csid') \
		return {tonumber(ret[1] or 0),tonumber(ret[2] or 0)}";
	RegRedisScript(PRO_JOIN_GAME,szPro);

	szPro = "local uid = tonumber(KEYS[1]) \
		redis.call('HMSET',uid,'gid',0,'gsid',0,'gsno',0) \
		local cser = redis.call('HMGET',uid,'cid') \
		if tonumber(cser[1] or 0) == 0 then \
			redis.call('DEL',uid) \
		end";	
	RegRedisScript(PRO_QUIT_GAME,szPro);
	
	return true;
}

int  CUserRedis::Exec(UINT nType,void* pData,UINT nDataSize,void *pRet,UINT nRet)
{
	CloseReply();
	
	switch(nType)
	{
		case Mem::USER_LOGIN_REQ:
		{
			if(nDataSize != sizeof(Mem::UserLoginMemReq))
				return false;
			Mem::UserLoginMemReq* pUserInfo = (Mem::UserLoginMemReq*)pData;
			UserLoginReq(pUserInfo->nUserId,pUserInfo->nSerNo,pUserInfo->nSockNo,pRet,nRet);
			break;
		}
		case Mem::USER_LOGOUT_REQ:
		{
			Mem::UserLogOutMemReq* pReq = (Mem::UserLogOutMemReq*)pData;
			redisReply* reply = (redisReply*)redisCommand(m_pConn, "EVALSHA %s %d %d %d %d",m_RedisPro[PRO_LOGIN_OUT],3,pReq->nUserId,pReq->nSerNo,pReq->nSockNo);
			freeReplyObject(reply);
			break;
		}
		case Mem::USER_JOIN_GAME_REQ:
		{
			Mem::UserJoinGameReq* pReq = (Mem::UserJoinGameReq*)pData;
			UserJoinGame(pReq->nUserId, pReq->nGid, pReq->nGsid, pReq->nSeatNo, pRet, nRet);
			break;
		}
		case Mem::USER_QUIT_GAME_REQ:
		{
			Mem::UserQuitGameReq* pReq = (Mem::UserQuitGameReq*)pData;
			redisReply* reply = (redisReply*)redisCommand(m_pConn, "EVALSHA %s %d %d %d %d %d",m_RedisPro[PRO_QUIT_GAME],4,pReq->nUserId,0,0,0);
			freeReplyObject(reply);
			break;
		}
		default:
			break;
	}
	return true;
}
