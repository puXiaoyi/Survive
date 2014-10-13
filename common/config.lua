--配置管理,从配置中心数据库获取本进程相关的配置
--package.cpath = "Survive/?.so"
local Redis = require "lua/redis"
local Cjson = require "cjson"
local Sche = require "lua/sche"
--local Base64 = require "base64"

local toredis
local deployment

local function connect_to_redis(ip,port)
    toredis = nil
	Sche.Spawn(function ()
		while true do
			local err
			err,toredis = Redis.Connect(ip,port,
										function (redisconn)
											print("connection to config server close")
										end)
			if toredis then
				print("connect to config server success")
				break
			end
			print("try to connect to config server after 1 sec")			
			Sche.Sleep(1000)
		end
	end)	
end

local isInit
local deploy_key

local function Init(key,ip,port)
	if not isInit then
		isInit = true
		deploy_key = key
		connect_to_redis(ip,port)
		while not toredis do
			Sche.Sleep(100)
		end
		local err,result = toredis:Command("hmget deploy " .. key)
		if err or not result then
			toredis:Close()
			return false,err
		else
			deployment = Cjson.decode(result[1])
			toredis:Close()
			return true,nil
		end
	else
		return "already init",false
	end
end

local function Get(key)
	if not deployment then
		return nil
	end
	return deployment[key]
end

return {
	Init = Init,
	Get = Get
}
