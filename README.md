# Tiger® Template

## How to build

##### Prerequisites：

1. cmake；
2. Visual Studio 2022
3. Tiger库；
4. Qt库；
5. Protobuf库；

###### 使用dev2022中的Openti，Qt与Protobuf库：

公司网盘中提供了dev_tools库，使用方法为：

1. 将`<smb dir>/dev_tools`文件夹拷贝到本地任意位置（前置路径尽量不要使用特殊字符、中文或空格）;
2. 进入`<your dir>/dev_tools`文件夹，双击运行`setvars.bat`将会设置个人环境变量，cmake即可找到对应的库文件；

###### 使用其他位置的库：

系统利用cmake自动查找所需库，要让cmake能正确的找到所需的库文件，需要正确的设置搜索文件夹：

1. Tiger的搜索文件夹为：`TigerConfig.cmake`所在文件夹，通常为库安装文件夹根目录；
2. Protobuf的搜索文件夹为：`protobuf-config.cmake`所在文件夹，通常为`<your>/<Protobuf>/<dir>/cmake`，设置为库安装文件夹根目录也可以；
3. Qt的搜索文件夹为：`Qt5Config.cmake`所在文件夹，通常为`<your>/<Qt>/<dir>/lib/cmake/Qt5/`

有三种方式可以让cmake找到对应库的位置：

1. 添加环境变量`TIGER_DIR`，`PROTOBUF_DIR` 与`QT_DIR`，并设置为各自的搜索文件夹；
2. 在根目录中新增文件`local.cmake`，并添加命令`set(XXX_SERACH_DIR <your>/<dir>)`即可（XXX_SEARCH_DIR 可能有：TIGER_SEARCH_DIR、PROTOBUF_SEARCH_DIR、QT_SEARCH_DIR）；
3. 不设置任何环境变量，在cmake-gui或cmake命令行中手动指定库位置；

##### 预编译

###### 使用cmake-gui

1. 点击Browse Source，选择代码文件夹；
2. 点击Browse Build，选择生成构建文件的文件夹，构建文件夹最好不要和代码文件夹相同，如：`<your source dir>/build`；
3. 点击Configure，进行构建；
4. 注意中间变量窗口中的红色/错误选项；
5. 配置好项目后，再次点击Configure，直到出现Configuring done；
6. 点击Generate，生成构建文件；
7. 点击Open Project，打开Visual Studio，解决方案与依赖已自动配置完成；
8. 构建CMakePredefinedTargets->ALL_BUILD，即可完成编译

###### 使用cmake命令行

略