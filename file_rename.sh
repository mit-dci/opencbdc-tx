#!/bin/bash

search_terms=("3PC" "threepc" "3pc" "programmability" "Programmability")
replacement="parsec"

find_result=$(find . -depth -name "*threepc*" -not -iwholename '*.git*')
while IFS= read -r entry; do
    name=$(basename "$entry")
    for ((i = 0; i < ${#search_terms[@]}; i++)); do
        search_term=${search_terms[i]}

        if [[ "$name" == *"${search_term}"* ]]; then
            new_name="${name//$search_term/$replacement}"

            mv "$entry" "$(dirname "$entry")/$new_name"
            echo "$(dirname "$entry")"
            echo "Renamed: $entry -> $new_name"
            break
        fi
    done
done <<< "$find_result"

find_result=$(find . -depth -name "*3PC*" -not -iwholename '*.git*')
while IFS= read -r entry; do
    name=$(basename "$entry")
    for ((i = 0; i < ${#search_terms[@]}; i++)); do
        search_term=${search_terms[i]}

        if [[ "$name" == *"${search_term}"* ]]; then
            new_name="${name//$search_term/$replacement}"

            mv "$entry" "$(dirname "$entry")/$new_name"

            echo "Renamed: $entry -> $new_name"
            break
        fi
    done
done <<< "$find_result"

find_result=$(find . -depth -name "*3pc*" -not -iwholename '*.git*')
while IFS= read -r entry; do
    name=$(basename "$entry")
    for ((i = 0; i < ${#search_terms[@]}; i++)); do
        search_term=${search_terms[i]}

        if [[ "$name" == *"${search_term}"* ]]; then
            new_name="${name//$search_term/$replacement}"

            mv "$entry" "$(dirname "$entry")/$new_name"

            echo "Renamed: $entry -> $new_name"
            break
        fi
    done
done <<< "$find_result"

find_result=$(find . -depth -name "*programmability*" -not -iwholename '*.git*')
while IFS= read -r entry; do
    name=$(basename "$entry")
    for ((i = 0; i < ${#search_terms[@]}; i++)); do
        search_term=${search_terms[i]}

        if [[ "$name" == *"${search_term}"* ]]; then
            new_name="${name//$search_term/$replacement}"

            mv "$entry" "$(dirname "$entry")/$new_name"
            echo "$(dirname "$entry")"
            echo "Renamed: $entry -> $new_name"
            break
        fi
    done
done <<< "$find_result"

find_result=$(find . -depth -name "*Programmability*" -not -iwholename '*.git*')
while IFS= read -r entry; do
    name=$(basename "$entry")
    for ((i = 0; i < ${#search_terms[@]}; i++)); do
        search_term=${search_terms[i]}

        if [[ "$name" == *"${search_term}"* ]]; then
            new_name="${name//$search_term/$replacement}"

            mv "$entry" "$(dirname "$entry")/$new_name"
            echo "$(dirname "$entry")"
            echo "Renamed: $entry -> $new_name"
            break
        fi
    done
done <<< "$find_result"
