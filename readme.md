##说明
广州可道技术公司 Stateam团队 开发
trafficserver的限速模块

##功能
分时段 单线程限速
##编译
在有ats编译环境的机器执行：
make
会生成ratemaster.so
##使用
配置ratemaster.so的具体路径到ats的plugin.config
限速的配置文件是写死路径的，配置文件是/sysconfig/limitconfig.config

###配置内容示例
limit_on=1    
max_rate=1    
0hour=1048576    
1hour=1048576    
2hour=1048576    
3hour=1048576    
4hour=1048576    
5hour=1048576    
6hour=1048576    
7hour=1048576    
8hour=1048576    
9hour=1048576    
10hour=1048576    
11hour=1048576    
12hour=1048576    
13hour=1048576    
14hour=1048576    
15hour=1048576    
16hour=1048576    
17hour=1048576    
18hour=1048576    
19hour=1048576    
20hour=1048576    
21hour=1048576    
22hour=1048576    
23hour=1048576    


limit_on=1 代表开启限速
max_rate=1 代表最大速度为1M
0hour=1048576 代表00点 限速为1048576字节/s

###大大广告
微信公众号：   
![image2](https://github.com/cnkedao/QQrobot/raw/master/pic/2.jpg) 