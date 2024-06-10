root_dir=$(git rev-parse --show-toplevel)
build_dir=${root_dir}/build
cmake -S ${root_dir} -B ${build_dir}
cd ${build_dir}
make -j32
cd -

