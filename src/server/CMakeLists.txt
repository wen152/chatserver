#定义SRC_LIST 包含该目录下的所有源文件
aux_source_directory(. SRC_LIST)
aux_source_directory(./database DB_LIST)
aux_source_directory(./model MODEL_LIST)
aux_source_directory(./redis REDIS_LIST)

#指定可执行文件
add_executable(ChatServer ${SRC_LIST} ${DB_LIST} ${MODEL_LIST} ${REDIS_LIST})

#指定可执行文件的依赖库 前后有依赖关系
target_link_libraries(ChatServer
        muduo_net
        muduo_base
        mysqlclient
        hiredis
        pthread
)