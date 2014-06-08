#include "agent.h"
#include "common/cmdhandler.h"
#include "netcmd.h"
#include "gateplayer.h"
#include "chanmsg.h"
#include "togrpgame.h"

static const uint16_t MAXCMD = 65536;

static cmd_handler_t handler[MAXCMD] = {NULL}

static __thread agent  t_agent = NULL;

static void forward_game(kn_stream_conn_t con,rpacket_t rpk){
	agentplayer_t ply = (agentplayer_t)kn_stream_conn_getud(con);
	wpacket_t wpk = wpk_create_by_rpacket(rpk);
	wpk_write_uint32(wpk,ply->gameid);
	struct chanmsg_forward_game *msg = calloc(1,sizeof(*msg));
	msg->chanmsg.msgtype = FORWARD_GAME;
	msg->game = ply->togame;
	msg->wpk = wpk;
	kn_channel_putmsg(g_togrpgame->chan,NULL,msg,chanmsg_forward_game_destroy);
}

static void forward_group(kn_stream_conn_t con,rpacket_t rpk){
	agentplayer_t ply = (agentplayer_t)kn_stream_conn_getud(con);
	wpacket_t wpk = wpk_create_by_rpacket(rpk);
	wpk_write_uint32(wpk,ply->groupid);
	struct chanmsg_forward_group *msg = calloc(1,sizeof(*msg));
	msg->chanmsg.msgtype = FORWARD_GROUP;
	msg->wpk = wpk;
	kn_channel_putmsg(g_togrpgame->chan,NULL,msg,chanmsg_forward_group_destroy);
}


//�������Կͻ��˵������
static int on_packet(kn_stream_conn_t con,rpacket_t rpk){
	uint16_t cmd = rpk_peek_uint16(rpk);
	if(cmd > CMD_CA_BEGIN && cmd < CMD_CA_END){
		rpk_read_uint16(rpk);
		if(handler[cmd]->_fn) handler[cmd]->_fn(rpk,con);
	}else if(cmd > CMD_CS_BEGIN && cmd < CMD_CS_END){
		 //ת����gameserver
		forward_game(con,rpk);
	}else if(cmd > CMD_CG_BEGIN && cmd < CMD_CG_END){
		//ת����groupserver
		forward_group(con,rpk);
	}
	return 1;
}

//��������channel����Ϣ
static void on_channel_msg(kn_channel_t chan, kn_channel_t from,void *msg,void *_)
{
	(void)_;

}

static void on_redis_connect(redisconn_t conn,int err,void *ud){
	if(conn) ((agent*)ud)->redis = conn;
	else{
		//����
	}
}

static	void on_redis_disconnected(redisconn_t conn,void *ud){
	((agent*)ud)->redis = NULL;
	//����
}

static void *service_main(void *ud){
	printf("agent service����\n");	
	t_agent = (agent*)ud;
	if(0 != kn_redisAsynConnect(t_agent->p,
		"127.0.0.1",8010,
		on_redis_connect,
		on_redis_disconnected,
		t_agent)){
		//��¼��־
		return NULL;
	}
	while(!t_agent->stop){
		kn_proactor_run(t_agent->p,50);
	}
	return NULL;
}


agent *start_agent(uint8_t idx){
	agent *agent = calloc(1,sizeof(*agent));
	agent->idx = idx;
	agent->p = kn_new_proactor();
	agent->t = kn_create_thread(JOINABLE);
	kn_new_stream_server(agent->p,NULL,NULL);
	agent->chan = kn_new_channel(kn_thread_getid(agent->t));
	kn_channel_bind(agent->p,agent->chan,on_channel_msg);
	kn_thread_start_run(agent->t,service_main,agent);
	return agent;
}

void   stop_agent(agent *agent){
	agent->stop = 1;
	kn_thread_join(agent->t);
	//stop_agentӦ���ڽ��̽���ʱ���ã������κ���β������
}