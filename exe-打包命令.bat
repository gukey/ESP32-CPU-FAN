rem 打包成“无控制台窗口”版本
pyinstaller -F -w -i Only.ico 2.py

rem 关键点
rem -w 参数

rem 这个就是关键

rem 有 -w → 不弹黑窗口
rem 没 -w → 就像你现在这样弹 cmd 窗口


rem  完整命令
rem  pyinstaller -F -w -i Only.ico -n FanController --clean 2.py