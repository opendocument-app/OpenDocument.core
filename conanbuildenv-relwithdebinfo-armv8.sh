script_folder="/private/tmp/claude-501/-Users-andreas-odr-OpenDocument-core/50040636-38e1-4d0f-90ae-6ed308703180/scratchpad/wt776"
echo "echo Restoring environment" > "$script_folder/deactivate_conanbuildenv-relwithdebinfo-armv8.sh"
for v in 
do
   is_defined="true"
   value=$(printenv $v) || is_defined="" || true
   if [ -n "$value" ] || [ -n "$is_defined" ]
   then
       echo export "$v='$value'" >> "$script_folder/deactivate_conanbuildenv-relwithdebinfo-armv8.sh"
   else
       echo unset $v >> "$script_folder/deactivate_conanbuildenv-relwithdebinfo-armv8.sh"
   fi
done
