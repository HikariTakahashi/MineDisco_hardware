#ifndef STYLE_H
#define STYLE_H

#include <Arduino.h>

// CSS（<style>タグの中身）をPROGMEM（フラッシュメモリ）に格納
const char STYLE_CSS[] PROGMEM = R"rawliteral(
  body {
    font-family: Arial, sans-serif;
    text-align: center;
    background-color: #f0f0f0;
    margin: 0;
    padding: 20px;
  }
  h1 {
    color: #333;
    margin-bottom: 30px;
    border-bottom: 2px solid #ccc;
    padding-bottom: 10px;
  }
  .container {
    display: flex;
    justify-content: center;
    gap: 30px;
    flex-wrap: wrap;
  }
  .room {
    border: 1px solid #ccc;
    background-color: white;
    padding: 20px;
    width: 450px; /* 部屋の幅 */
    height: 600px; /* 高さを600pxに維持 */
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    position: relative;
  }
  .room-name {
    position: absolute;
    top: 10px;
    left: 50%;
    transform: translateX(-50%);
    font-weight: bold;
    font-size: 1.2em;
    color: #555;
  }
  .grid-container {
    display: grid;
    grid-template-rows: repeat(8, 50px); 
    gap: 10px;
    width: 100%;
    justify-items: center;
    padding-top: 40px; /* 部屋名との間隔 */
  }
  /* 2-301 は 3列グリッド */
  .room-301 .grid-container {
    grid-template-columns: repeat(3, 1fr); 
  }
  /* 2-302 は 5列グリッド */
  .room-302 .grid-container {
    grid-template-columns: repeat(5, 1fr); 
  }
  
  .grid-item {
    width: 80px; /* 四角のサイズ (幅) */
    height: 50px; /* 四角のサイズ (高さ) */
    border: 1px solid black;
    background-color: #eee;
    display: flex;
    align-items: center;
    justify-content: center;
    font-weight: bold;
    font-size: 1.2em;
    color: #333;
  }
  /* 5列グリッド用に幅を調整 */
  .room-302 .grid-item {
    width: 60px; 
  }
  
  .highlighted {
    background-color: #ffcccc; /* ハイライト色 (薄い赤) */
    border: 1px solid red;
    color: #cc0000; /* ハイライト時の文字色 */
  }

  /* 2-301 の配置 (3列グリッド) (grid-area: row-start / col-start / row-end / col-end) */
  /* 左列（Col 1、上から下）: 1, 2, ▫️, 8, 12, ▫️, 11 */
  .room-301 .grid-item:nth-child(1) { grid-area: 1 / 1 / 2 / 2; } /* Row 1, Col 1: 1 */
  .room-301 .grid-item:nth-child(2) { grid-area: 2 / 1 / 3 / 2; } /* Row 2, Col 1: 2 */
  .room-301 .grid-item:nth-child(3) { grid-area: 3 / 1 / 4 / 2; } /* Row 3, Col 1: ▫️ */
  .room-301 .grid-item:nth-child(4) { grid-area: 4 / 1 / 5 / 2; } /* Row 4, Col 1: 8 */
  .room-301 .grid-item:nth-child(5) { grid-area: 5 / 1 / 6 / 2; } /* Row 5, Col 1: 12 */
  .room-301 .grid-item:nth-child(6) { grid-area: 6 / 1 / 7 / 2; } /* Row 6, Col 1: ▫️ */
  .room-301 .grid-item:nth-child(7) { grid-area: 7 / 1 / 8 / 2; } /* Row 7, Col 1: 11 */
  /* 中央列（Col 2、上から下）: ◾️(なし), 10, ◾️(なし), 9, ◾️(なし), ◾️(なし), ◾️(なし) */
  .room-301 .grid-item:nth-child(8) { grid-area: 2 / 2 / 3 / 3; } /* Row 2, Col 2: 10 */
  .room-301 .grid-item:nth-child(9) { grid-area: 4 / 2 / 5 / 3; } /* Row 4, Col 2: 9 */
  /* 右列（Col 3、上から下）: 3, 4, ▫️, 5, ▫️, ▫️, 6 */
  .room-301 .grid-item:nth-child(10) { grid-area: 1 / 3 / 2 / 4; } /* Row 1, Col 3: 3 */
  .room-301 .grid-item:nth-child(11) { grid-area: 2 / 3 / 3 / 4; } /* Row 2, Col 3: 4 */
  .room-301 .grid-item:nth-child(12) { grid-area: 3 / 3 / 4 / 4; } /* Row 3, Col 3: ▫️ */
  .room-301 .grid-item:nth-child(13) { grid-area: 4 / 3 / 5 / 4; } /* Row 4, Col 3: 5 */
  .room-301 .grid-item:nth-child(14) { grid-area: 5 / 3 / 6 / 4; } /* Row 5, Col 3: ▫️ */
  .room-301 .grid-item:nth-child(15) { grid-area: 6 / 3 / 7 / 4; } /* Row 6, Col 3: ▫️ */
  .room-301 .grid-item:nth-child(16) { grid-area: 7 / 3 / 8 / 4; } /* Row 7, Col 3: 6 */

  /* 2-302 の配置 (5列グリッド) */
  /* 左列（Col 1、上から下）: 5, ▫️, ▫️, ▫️, 1, ▫️, ▫️ */
  .room-302 .grid-item:nth-child(1) { grid-area: 1 / 1 / 2 / 2; } /* Row 1, Col 1: 5 */
  .room-302 .grid-item:nth-child(2) { grid-area: 2 / 1 / 3 / 2; } /* Row 2, Col 1: ▫️ */
  .room-302 .grid-item:nth-child(3) { grid-area: 3 / 1 / 4 / 2; } /* Row 3, Col 1: ▫️ */
  .room-302 .grid-item:nth-child(4) { grid-area: 4 / 1 / 5 / 2; } /* Row 4, Col 1: ▫️ */
  .room-302 .grid-item:nth-child(5) { grid-area: 5 / 1 / 6 / 2; } /* Row 5, Col 1: 1 */
  .room-302 .grid-item:nth-child(6) { grid-area: 6 / 1 / 7 / 2; } /* Row 6, Col 1: ▫️ */
  .room-302 .grid-item:nth-child(7) { grid-area: 7 / 1 / 8 / 2; } /* Row 7, Col 1: ▫️ */
  /* Col 2（上から下）: ◾️(なし), ◾️(なし), ◾️(なし), ◾️(なし), ◾️(なし), ◾️(なし), 2 */
  .room-302 .grid-item:nth-child(8) { grid-area: 7 / 2 / 8 / 3; } /* Row 7, Col 2: 2 */
  /* 中央列（Col 3、上から下）: ◾️(なし), ▫️, ◾️(なし), ▫️, ◾️(なし), ◾️(なし), ▫️ */
  .room-302 .grid-item:nth-child(9) { grid-area: 2 / 3 / 3 / 4; } /* Row 2, Col 3: ▫️ */
  .room-302 .grid-item:nth-child(10) { grid-area: 4 / 3 / 5 / 4; } /* Row 4, Col 3: ▫️ */
  .room-302 .grid-item:nth-child(11) { grid-area: 7 / 3 / 8 / 4; } /* Row 7, Col 3: ▫️ */
  /* Col 4（上から下）: ◾️(なし), ◾️(なし), ◾️(なし), ◾️(なし), ◾️(なし), ◾️(なし), 2 */
  .room-302 .grid-item:nth-child(12) { grid-area: 7 / 4 / 8 / 5; } /* Row 7, Col 4: 2 */
  /* 右列（Col 5、上から下）: 6, 7, ▫️, 4, ▫️, ▫️, ▫️ */
  .room-302 .grid-item:nth-child(13) { grid-area: 1 / 5 / 2 / 6; } /* Row 1, Col 5: 6 */
  .room-302 .grid-item:nth-child(14) { grid-area: 2 / 5 / 3 / 6; } /* Row 2, Col 5: 7 */
  .room-302 .grid-item:nth-child(15) { grid-area: 3 / 5 / 4 / 6; } /* Row 3, Col 5: ▫️ */
  .room-302 .grid-item:nth-child(16) { grid-area: 4 / 5 / 5 / 6; } /* Row 4, Col 5: 4 */
  .room-302 .grid-item:nth-child(17) { grid-area: 5 / 5 / 6 / 6; } /* Row 5, Col 5: ▫️ */
  .room-302 .grid-item:nth-child(18) { grid-area: 6 / 5 / 7 / 6; } /* Row 6, Col 5: ▫️ */
  .room-302 .grid-item:nth-child(19) { grid-area: 7 / 5 / 8 / 6; } /* Row 7, Col 5: ▫️ */
)rawliteral";

#endif