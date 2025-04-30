mkdir docs/resources/
for relatorio in relatorio_*
do
    cp $relatorio/resources/* docs/resources/
done

return 0 #added just to supress the error status from folders without resources