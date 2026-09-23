- timestamp는 YYYY-MM-DD HH:mm 형식 / 2026-01-01 부터 2026-04-30 까지 있음.
- tag는 바이오마커들 추가하면 좋을 것 같아서 일단은 비워둠.


event.csv
- 칼럼: timestamp,end_timestamp,activity_type,activity_summary,member,location,tag

meal.csv
- 칼럼: timestamp,meal_type,item,kcal,protein_g,carbs_g,fat_g,tag

biodata.csv
- 칼럼: timestamp,member,biomarker,value,tag
- 일단 바이오 마커 5개, 멤버는 A 한 명으로 함.
