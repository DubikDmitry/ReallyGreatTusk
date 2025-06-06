#include <stdio.h>
#include <stdlib.h> 
#include <strings.h>
#include <math.h> 
#include "lodepng.h" 

//Сначала добавим структуру пикселей, чтобы затем работать с их интенсивностью цвета для сортировки

typedef struct{
    int x, y; //Координаты пикселя
}Pixel;

//Теперь добавим рёбра

typedef struct{
    Pixel p1, p2; //Два пикселя, между которыми ведёт ребро
    int diff; //Интенсивность цвета между рёбрами, по которым в дальнейшем будем соединять
}Edge;

//Далее будем использовать непересекающиеся множества для работы со связностью на ших рёбер в остовное дерево

typedef struct{
    int parent; //Индекс родительского элемента. Предлагаю для работы массив. Дальше будет понятно
    int rank; //Под рангом будем понимать глубину дерева. Далее будем использовать для объединения меньшего к большему дереву
}Subset;

//Напишем стандартные функции find и Union для работы с множествами. (Функцию удаления не пишу, он в работе как-будто бы не нужна)
//Функция find будет получать на вход массив с элементами, которые являются отдельными множествами с указателями на родителей\
Когда увидим корневой элемент (он сам себе родитель), то начнём к нему подвешивать всех

int find(Subset subsets[], int i){
    if (subsets[i].parent != i){
        subsets[i].parent = find(subsets, subsets[i].parent); //Если не родитель, то ищем дальше
    }
    
    return subsets[i].parent; //Возвращаем Всеотца
}

//Функция Union будет объединять подмножества по принципу меньший к большему\
Если глубины одинаковы, то прикрепляемся и ранг на 1 повышаем
//Важно, subsets - это массив подмножеств (всех) и тут я хоч объединить какие-то элементы х и у в нём\
х и у - это именно подмножества внутри массива всех множеств!
void Union(Subset subsets[], int x, int y) {
    int xroot = find(subsets, x); //Изем корень первого
    int yroot = find(subsets, y); //Ищем корень второго

    //Объединяем по рангу
    if (subsets[xroot].rank < subsets[yroot].rank)
        subsets[xroot].parent = yroot;
    else if (subsets[xroot].rank > subsets[yroot].rank)
        subsets[yroot].parent = xroot;
    else {
        subsets[yroot].parent = xroot;
        subsets[xroot].rank++;
    }
}

//Подготовив функции для работы с множествами можем приступать к самой картинке, а конкретно - к рёбрам между пикселям\
Я буду далее сортировать с помощью встроенного qsort(), поэтому будем использовать тип const void*, как требует функция

int edge_comparator(const void* a, const void* b) {
    Edge* a1 = (Edge*)a; //Приводим типы к ребру
    Edge* a2 = (Edge*)b;
    return a1->diff - a2->diff;//Сравниваем интенсивность цвета
}

//Перед следующей функцией сделаю замечание для qsort() туда я буду подавать\
массив рёбер, количество рёбер, размер рёбер(как размер структуры) и моё сравнение очевидно

//Итак, следующая функция будет обрабатывать половину изображения (правую или левую) и делить её\
на компоненты связности алгоритмом Краскала для дальнейше раскраски. Почему на половины?\
Потому что произошла ,видимо, череп слева слегка отличается по тональности от мозгов справа\
и как более менее адекватно раскрасить обе части не деля их я не придумал.

void process_half(unsigned char* image, int* labels, int start_x, int end_x, int width, int height, int half_offset) {
    int half_width = end_x - start_x; //Ширина половины (конец половины определим как ширина/2)
    int total_pixels = half_width * height; //Все пиксели (умножаем ширину на высоту)
    
    Edge* edges = (Edge*)malloc(half_width * height * 4 * sizeof(Edge)); //У каждого пикселя может быть 4 соседа 
    int edge_count = 0; //Количество рёбер

    
    for (int y = 0; y < height; y++) {
        for (int x = start_x; x < end_x; x++) {
            int idx = y * width + x;//Переводим в индекс (из двумерных в линейный)
            int intensity = (image[4*idx] + image[4*idx+1] + image[4*idx+2]) / 3; //Усредняем цвет каждого пикселя (да прямо в лоб)

            //Сосед справа (смотрим что он не заходит за правую границу)
            if (x < end_x-1) { 
                int right_idx = y * width + (x+1);
                int right_intensity = (image[4*right_idx] + image[4*right_idx+1] + image[4*right_idx+2]) / 3;
                //Добавляем рёбра в массив не забывая указать их вес (про вес смотрите выше)
                edges[edge_count].p1.x = x;
                edges[edge_count].p1.y = y;
                edges[edge_count].p2.x = x+1;
                edges[edge_count].p2.y = y;
                edges[edge_count].diff = abs(intensity - right_intensity);
                edge_count++;
            }

            //Сосед сверху (аналогично)
            if (y < height-1) { 
                int bottom_idx = (y+1) * width + x;
                int bottom_intensity = (image[4*bottom_idx] + image[4*bottom_idx+1] + image[4*bottom_idx+2]) / 3;
                edges[edge_count].p1.x = x;
                edges[edge_count].p1.y = y;
                edges[edge_count].p2.x = x;
                edges[edge_count].p2.y = y+1;
                edges[edge_count].diff = abs(intensity - bottom_intensity);
                edge_count++;
            }
        }
    }

    //Сортируем по интенсивности цвета
    qsort(edges, edge_count, sizeof(Edge), edge_comparator);

    //Создаём массив непересекающихся множеств (каждый пиксель)
    Subset* subsets = (Subset*)malloc(total_pixels * sizeof(Subset));
    for (int i = 0; i < total_pixels; i++) {
        subsets[i].parent = i;
        subsets[i].rank = 0;
    }

    int threshold = 20; //Это граница допустимой разницы для добавление в связность (разница цвета рёбер)

    for (int i = 0; i < edge_count; i++) {
        //Ищем индексы пикселей, между которыми есть ребро
        int u = edges[i].p1.y * half_width + (edges[i].p1.x - start_x);
        int v = edges[i].p2.y * half_width + (edges[i].p2.x - start_x);

        //Находим корень множества
        int set_u = find(subsets, u);
        int set_v = find(subsets, v);

        //Если корни разные, но разница между ними мала -> объединяем
        if (set_u != set_v && edges[i].diff < threshold) {
            Union(subsets, set_u, set_v);
        }
    }

    //Строим наши компоненты связности в массиве (Теперь каждому пикселю множества будет стоять своя метка)\
    одинаковые метки -> одинаково красим
    for (int y = 0; y < height; y++) {
        for (int x = start_x; x < end_x; x++) {
            int idx = y * width + x;
            int subset_idx = y * half_width + (x - start_x);
            labels[idx] = find(subsets, subset_idx) + half_offset; //half_offset нужен, чтобы правое с левым не пересекалось 
        }
    }

    free(edges);
    free(subsets);
}


//Теперь будем красить! После обработки половины изображения мы можем эту половину покрасит\
Смысл прост. Берём наш массив меток, полученный ранее и раскрашиваем пиксели каждой метки в нужный цвет\
Так как я хочу раскрасить череп, мозг и фон, я разбиваю условие на три части и крашу в нужный мне цвет

void colorize_half(unsigned char* result, int* labels, int start_x, int end_x, int width, int height, int color1, int color2) {
    //Разбиваем и зображение по тому же принципу
    int half_width = end_x - start_x;
    int total_pixels = half_width * height;

    //Ищем количество меток (оно равно максимальной +1)
    int max_label = 0;
    for (int y = 0; y < height; y++) {
        for (int x = start_x; x < end_x; x++) {
            int idx = y * width + x;
            if (labels[idx] > max_label) max_label = labels[idx];
        }
    }

    int* component_size = (int*)calloc(max_label+1, sizeof(int)); //Количество меток в компоненте i
    int* component_sum = (int*)calloc(max_label+1, sizeof(int)); //Суммарная интенсивность цвета в компоненте i

    //Заполняем их
    for (int y = 0; y < height; y++) {
        for (int x = start_x; x < end_x; x++) {
            int idx = y * width + x;
            int label = labels[idx];
            int intensity = (result[4*idx] + result[4*idx+1] + result[4*idx+2]) / 3;
            component_sum[label] += intensity;
            component_size[label]++;
        }
    }

    //Теперь сама раскраска. Возьмём оригинальную интенсивность и среднюю интенсивность по множеству\
    Это будет нужно для плавной раскраски (иначе получаются точки, как я вам показывал)\
    Среднюю интенсивность опять же берём в лоб, а итоговую обозначаем как сумму части оригинальной и средней интенсивности
    for (int y = 0; y < height; y++) {
        for (int x = start_x; x < end_x; x++) {
            int idx = y * width + x;
            int label = labels[idx];
            float avg_intensity = (float)component_sum[label] / component_size[label];
            float original_intensity = (result[4*idx] + result[4*idx+1] + result[4*idx+2]) / 3.0f;
            
            float blend = 0.5f;
            float blended_intensity = blend * original_intensity + (1.0f - blend) * avg_intensity;

            //Самым ярким сделаем мозг color1 > color2
            if (blended_intensity > color1) {
                result[4*idx] = 50 + blended_intensity * 0.5f;
                result[4*idx+1] = 150 + blended_intensity * 0.3f;
                result[4*idx+2] = 50 + blended_intensity * 0.2f;
            } //Затем череп
            else if (blended_intensity > color2) { // Skull
                result[4*idx] = 120 + blended_intensity * 0.5f;
                result[4*idx+1] = 100 + blended_intensity * 0.4f;
                result[4*idx+2] = 30 + blended_intensity * 0.2f;
            }
            else { //А вот и фон
                result[4*idx] = 60 + blended_intensity * 0.4f;
                result[4*idx+1] = 20 + blended_intensity * 0.1f;
                result[4*idx+2] = 20 + blended_intensity * 0.1f;
            }
            result[4*idx+3] = 255;
        }
    }

    free(component_size);
    free(component_sum);
}







//Дальнейшие стандартные функции я просто взял с вашего гитхаба Михаил Владимирович
// принимаем на вход: имя файла, указатели на int для хранения прочитанной ширины и высоты картинки
// возвращаем указатель на выделенную память для хранения картинки
// Если память выделить не смогли, отдаем нулевой указатель и пишем сообщение об ошибке
unsigned char* load_png(const char* filename, unsigned int* width, unsigned int* height) 
{
  unsigned char* image = NULL; 
  int error = lodepng_decode32_file(&image, width, height, filename);
  if(error != 0) {
    printf("error %u: %s\n", error, lodepng_error_text(error)); 
  }
  return (image);
}

// принимаем на вход: имя файла для записи, указатель на массив пикселей,  ширину и высоту картинки
// Если преобразовать массив в картинку или сохранить не смогли,  пишем сообщение об ошибке
void write_png(const char* filename, const unsigned char* image, unsigned width, unsigned height)
{
  unsigned char* png;
  long unsigned int pngsize;
  int error = lodepng_encode32(&png, &pngsize, image, width, height);
  if(error == 0) {
      lodepng_save_file(png, pngsize, filename);
  } else { 
    printf("error %u: %s\n", error, lodepng_error_text(error));
  }
  free(png);
}

int main() {
    const char* filename = "Task.png";
    unsigned int width, height;

    unsigned char* image = load_png(filename, &width, &height);
    if (image == NULL) {
        printf("Problem reading picture from the file %s. Error.\n", filename);
        return -1;
    }

    //Объявляем кол-во пикселей, результатное изображение, массив меток (компонент связности)
    int total_pixels = width * height;
    unsigned char* result = (unsigned char*)malloc(4 * total_pixels * sizeof(unsigned char));
    int* labels = (int*)malloc(total_pixels * sizeof(int));

    //Будем работать с копией, чтобы изображение не портить
    memcpy(result, image, 4 * total_pixels * sizeof(unsigned char));

    //Находим середину
    int mid_x = width / 2;

    //Левая часть
    process_half(image, labels, 0, mid_x, width, height, 0);
    colorize_half(result, labels, 0, mid_x, width, height, 80, 65);

    //Правая часть
    process_half(image, labels, mid_x, width, width, height, mid_x * height);
    colorize_half(result, labels, mid_x, width, width, height, 100, 80);

    write_png("NewBrain.png", result, width, height);

    free(image);
    free(result);
    free(labels);

    return 0;
}













  