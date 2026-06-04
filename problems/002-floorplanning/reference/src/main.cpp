#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <utility>
#include <stack>
#include <algorithm>
#include <list>
#include <sstream> 
#include <cmath>
#include <climits>
#include <queue>
#include <algorithm>
#include <time.h>
#include <unordered_map>

using namespace std;

vector<string> tokenize(string const &str) 
{ 
    vector <string> out; 
    istringstream ss(str);
    string word;
    while (ss >> word) 
    {
        out.push_back(word);
    }
    return out;
}

class block
{
public:
    string name;
    int x;
    int y;
    int width;
    int height;
    int min_area;
    vector<pair<int, int>> shape;
    int temp_x;
    int temp_y;
    int temp_width;
    int temp_height;

    block (string name, int x, int y, int width, int height)
    {
        this->name = name;
        this->x = x;
        this->y = y;
        this->width = width;
        this->height = height;
    };
    block (string name, int min_area)
    {
        this->name = name;
        this->min_area = min_area;
        int min_width = ceil(sqrt(min_area / 2));
        int max_width = floor(sqrt(2 * min_area));
        // only want at most 20 shapes
        int step = (max_width - min_width) / 20;
        for(int i = min_width; i <= max_width; i += (step < 1 ? 1 : step)){
            this->shape.push_back(make_pair(i, ceil((float)min_area / i)));
        }
    };
    ~block (){};

    void backup(){
        this->temp_x = this->x;
        this->temp_y = this->y;
        this->temp_width = this->width;
        this->temp_height = this->height;
    };
};

void set_grid(vector<vector<bool>>& Grid, int x, int y, int width, int height, bool value){
    for(int i = y; i < y + height; i++){
        for(int j = x; j < x + width; j++){
            Grid[i][j] = value;
        }
    }
}

bool check_placed(int x, int y, int width, int height, vector<vector<bool>>& Grid){
    if(x + width > Grid[0].size() || y + height > Grid.size()){
        return false;
    }
    for(int i = y; i < y + height; i++){
        for(int j = x; j < x + width; j++){
            if(Grid[i][j]){
                return false;
            }
        }
    }
    return true;
}

pair<int, int> compact(int x, int y, int width, int height, vector<vector<bool>>& Grid){
    int new_x = x;
    int new_y = y;
    // check every row below the current row is available
    for(int i = y - 1; i >= 0; i--){
        bool row_available = true;
        for(int j = x; j < x + width; j++){
            if(Grid[i][j]){
                row_available = false;
                break;
            }
        }
        if(!row_available) break;
        new_y = i;
    }
    // check every column to the left of the current column is available
    for(int i = x - 1; i >= 0; i--){
        bool col_available = true;
        for(int j = new_y; j < new_y + height; j++){
            if(Grid[j][i]){
                col_available = false;
                break;
            }
        }
        if(!col_available) break;
        new_x = i;
    }
    // return result
    return make_pair(new_x, new_y);
}

bool placed_block(block& module, vector<vector<bool>>& Grid){
    for(int y = 0; y < Grid.size(); y += (Grid.size() < 20 ? 1 : rand() % 11 + 20)){
        for(int x = 0; x < Grid[0].size(); x += (Grid[0].size() < 20 ? 1 : rand() % 11 + 20)){
            if(!Grid[y][x]){
                for(auto it: module.shape){
                    if(check_placed(x, y, it.first, it.second, Grid)){
                        pair<int, int> new_pos = compact(x, y, it.first, it.second, Grid);
                        module.x = new_pos.first;
                        module.y = new_pos.second;
                        module.width = it.first;
                        module.height = it.second;
                        set_grid(Grid, module.x, module.y, module.width, module.height, true);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void reset_block(vector<block*> &SoftModuleVector, vector<vector<bool>>& Grid){
    for(auto &it: SoftModuleVector){
        if(it->width == 0 || it->height == 0){
            break;
        }
        for(int i = it->y; i < it->y + it->height; i++){
            for(int j = it->x; j < it->x + it->width; j++){
                Grid[i][j] = false;
            }
        }
        it->x = 0;
        it->y = 0;
        it->width = 0;
        it->height = 0;
    }
}

bool swap_module(block& module1, block& module2, vector<vector<bool>>& Grid){
    // check if after swapping, module is overlapping with any of the modules
    set_grid(Grid, module1.x, module1.y, module1.width, module1.height, false);
    set_grid(Grid, module2.x, module2.y, module2.width, module2.height, false);
    // two step check
    if(check_placed(module1.x, module1.y, module2.width, module2.height, Grid)){
        // place module2 at module1's position
        set_grid(Grid, module1.x, module1.y, module2.width, module2.height, true);
        // after placing, check if module1 can be placed at module2's original position
        if(check_placed(module2.x, module2.y, module1.width, module1.height, Grid)){
            // if true, swap the modules
            set_grid(Grid, module2.x, module2.y, module1.width, module1.height, true);
            module1.backup();
            module2.backup();
            swap(module1.x, module2.x);
            swap(module1.y, module2.y);
            return true;
        }
        else{
            // if not, restore the grid
            set_grid(Grid, module1.x, module1.y, module2.width, module2.height, false);
        }
    }
    set_grid(Grid, module1.x, module1.y, module1.width, module1.height, true);
    set_grid(Grid, module2.x, module2.y, module2.width, module2.height, true);
    return false;
}

bool move_module(block& module, vector<vector<bool>>& Grid){
    // check if after moving, module is overlapping with any of the modules
    int r = rand() % 100;
    set_grid(Grid, module.x, module.y, module.width, module.height, false);
    // move up
    if(r < 50){
        // find a random position above the current position
        // random number : range from current y_coordinate to Grid.size() - module.height - current y_coordinate
        int new_y = rand() % (Grid.size() - module.height - module.y + 1) + module.y + module.height;
        if(check_placed(module.x, new_y, module.width, module.height, Grid)){
            module.backup();
            module.y = new_y;
            set_grid(Grid, module.x, module.y, module.width, module.height, true);
            return true;
        }
    }
    else{
    // move right
        // find a random position to the right of the current position
        // random number : range from current x_coordinate to Grid[0].size() - module.width
        int new_x = rand() % (Grid[0].size() - module.width - module.x + 1) + module.x + module.width;
        if(check_placed(new_x, module.y, module.width, module.height, Grid)){
            module.backup();
            module.x = new_x;
            set_grid(Grid, module.x, module.y, module.width, module.height, true);
            return true;
        }
    }
    set_grid(Grid, module.x, module.y, module.width, module.height, true);
    return false;
}

bool change_shape(block& module, vector<vector<bool>>& Grid){
    // check if after changing shape, module is overlapping with any of the modules
    // Set Grid to false
    set_grid(Grid, module.x, module.y, module.width, module.height, false);
    for(auto it: module.shape){
        if(it.first == module.width && it.second == module.height){
            continue;
        }
        if(check_placed(module.x, module.y, it.first, it.second, Grid)){
            module.backup();
            // if not, change the shape
            module.width = it.first;
            module.height = it.second;
            set_grid(Grid, module.x, module.y, module.width, module.height, true);
            return true;
        }
    }
    // Restore Grid
    set_grid(Grid, module.x, module.y, module.width, module.height, true);
    return false;
}

int HPWL(vector<pair<int, pair<block*, block*>>>& NetVector){
    int HPWL = 0;
    for(auto it: NetVector){
        // caculate each net's HPWL
        // first calculate the center of each block
        int x1, y1, x2, y2;
        x1 = it.second.first->x + it.second.first->width / 2;
        y1 = it.second.first->y + it.second.first->height / 2;
        x2 = it.second.second->x + it.second.second->width / 2;
        y2 = it.second.second->y + it.second.second->height / 2;
        // then calculate the HPWL
        HPWL += it.first * (abs(x1 - x2) + abs(y1 - y2));
    }
    return HPWL;
}

float cost_function(int ChipSizeWidth, int ChipSizeHeight, float alpha, float beta, vector<block*> &SoftModuleVector, vector<block*> &FixedModuleVector, vector<pair<int, pair<block*, block*>>>& NetVector, int& wirelength){
    int total_area = 0;
    for(auto it: SoftModuleVector){
        total_area += it->width * it->height;
    }
    for(auto it: FixedModuleVector){
        total_area += it->width * it->height;
    }
    wirelength = HPWL(NetVector);
    float cost = alpha * (float)total_area + beta * (float)wirelength;
    return cost;
}

vector<block*> gen_neighbor(vector<block*> &SoftModuleVector, vector<block*> &FixedModuleVector, vector<vector<bool>>& Grid, int r){
    vector<block*> changed;
    if(r < 33){
        // OP1 : Swap the positions of two randomly selected modules
        
            int index1 = rand() % SoftModuleVector.size();
            int index2 = rand() % SoftModuleVector.size();
            while(index1 == index2){
                index2 = rand() % SoftModuleVector.size();
            }
            if(!swap_module(*SoftModuleVector[index1], *SoftModuleVector[index2], Grid)){
                return vector<block*>();
            }
            else{
                changed.push_back(SoftModuleVector[index1]);
                changed.push_back(SoftModuleVector[index2]);
            }
        
    }
    else if(r < 66){
        // OP2 : Move a randomly selected module to a randomly selected position 
        
            int index = rand() % SoftModuleVector.size();
            if(!move_module(*SoftModuleVector[index], Grid)){
                return vector<block*>();
            }
            else{
                changed.push_back(SoftModuleVector[index]);
            }
        
    }
    else{
        // OP2 : Change the shape of a randomly selected module
        
            int index = rand() % SoftModuleVector.size();
            if(!change_shape(*SoftModuleVector[index], Grid)){
                return vector<block*>();
            }
            else{
                changed.push_back(SoftModuleVector[index]);
            }
        
    }
    return changed;
}

void restore(vector<block*> changed, vector<vector<bool>>& Grid){
    // let changed modules back to their original position and shape
    for(auto it : changed){
        set_grid(Grid, it->x, it->y, it->width, it->height, false);
    }  
    for(auto it : changed){
        it->x = it->temp_x;
        it->y = it->temp_y;
        it->width = it->temp_width;
        it->height = it->temp_height;
        set_grid(Grid, it->x, it->y, it->width, it->height, true);
    } 
}


void Parser(string input_file, string output_file){
    double start_time;
    start_time = clock();
    
    string myText;
    vector<string> v;

    ifstream MyReadFile(input_file);

    getline(MyReadFile, myText);
    v = tokenize(myText);
    int ChipSizeWidth = stoi(v[1]);
    int ChipSizeHeight = stoi(v[2]);

    getline(MyReadFile, myText); // skip empty line

    getline(MyReadFile, myText);
    v = tokenize(myText);
    int NumSoftModules = stoi(v[1]);

    vector<block*> SoftModuleVector;
    unordered_map<string, block*> SoftModuleMap;
    for(int i = 0; i < NumSoftModules; i++){
        getline(MyReadFile, myText);
        v = tokenize(myText);
        block* b = new block(v[1], stoi(v[2]));
        SoftModuleVector.push_back(b);
        SoftModuleMap[v[1]] = b;
    }

    getline(MyReadFile, myText); // skip empty line

    getline(MyReadFile, myText);
    v = tokenize(myText);

    int NumFixedModules = stoi(v[1]);
    vector<block*> FixedModuleVector;
    unordered_map<string, block*> FixedModuleMap;
    for(int i = 0; i < NumFixedModules; i++){
        getline(MyReadFile, myText);
        v = tokenize(myText);
        block *b = new block(v[1], stoi(v[2]), stoi(v[3]), stoi(v[4]), stoi(v[5]));
        FixedModuleVector.push_back(b);
        FixedModuleMap[v[1]] = b;
    }

    getline(MyReadFile, myText); // skip empty line

    getline(MyReadFile, myText);
    v = tokenize(myText);
    int NumNets = stoi(v[1]);

    vector<pair<int, pair<block*, block*>>> NetVector;

    for(int i = 0; i < NumNets; i++){
        getline(MyReadFile, myText);
        v = tokenize(myText);
        int weight = stoi(v[3]);
        block* b1 = SoftModuleMap.find(v[1]) != SoftModuleMap.end() ? SoftModuleMap[v[1]] : FixedModuleMap[v[1]];
        block* b2 = SoftModuleMap.find(v[2]) != SoftModuleMap.end() ? SoftModuleMap[v[2]] : FixedModuleMap[v[2]];
        NetVector.push_back(make_pair(weight, make_pair(b1, b2)));
    }
    MyReadFile.close();

    // Initial Placement

    // sort the soft modules in the decreasing order of their minimum areas
    sort(SoftModuleVector.begin(), SoftModuleVector.end(), [](block* a, block* b) {
        return a->min_area > b->min_area;
    });
    

    vector<vector<bool>> Grid(ChipSizeHeight, vector<bool>(ChipSizeWidth, false));
    
    for(auto it : FixedModuleVector){
        set_grid(Grid, it->x, it->y, it->width, it->height, true);
    }

    int i = 0;
    while(i != SoftModuleVector.size()){
        for(i = 0; i < SoftModuleVector.size(); i++){
            if(!placed_block(*SoftModuleVector[i], Grid)){
                reset_block(SoftModuleVector, Grid);
                break;
            }
        }
    }

    // Simulated annealing

    // OP1 : Swap the positions of two randomly selected modules
    // OP2 : Move a randomly selected module to a randomly selected position 
    // OP3 : Change the shape of a randomly selected module
    
    int wirelength;
    vector<block> best_sol;
    for(auto it: SoftModuleVector){
        block b(it->name, it->x, it->y, it->width, it->height);
        best_sol.push_back(b);
    }

    float alpha = 0.5 , beta = (1-alpha);	// alpha-area , beta-wirelength

    // Parameters
    double T = 1000.0, T_MIN = 1.0, T_DECAY = 0.95;
    double REJECT_RATIO = 0.95;
    int K = 20;
    int N = SoftModuleVector.size() * K;
    int DOUBLE_N = N * 2;

    // Variables
    float cost = cost_function(ChipSizeWidth, ChipSizeHeight, alpha, beta, SoftModuleVector, FixedModuleVector, NetVector, wirelength);
    int best_wirelength = wirelength;
    float min_cost = cost;
    int gen_cnt = 1, uphill_cnt = 0, reject_cnt = 0;

    // Simulated annealing
    
    while((double)reject_cnt / gen_cnt <= REJECT_RATIO && T >= T_MIN && ((clock() - start_time) / CLOCKS_PER_SEC <= 580)) {
        // Initialize
        gen_cnt = 0, uphill_cnt = 0, reject_cnt = 0;

        // Generate neighbor
        while(uphill_cnt <= N && gen_cnt <= DOUBLE_N && ((clock() - start_time) / CLOCKS_PER_SEC <= 580)) {
            int r = rand() % 100;
            // changed for restore
            vector<block*> changed = gen_neighbor(SoftModuleVector, FixedModuleVector, Grid, r);
            int neighbor_cost = cost_function(ChipSizeWidth, ChipSizeHeight, alpha, beta, SoftModuleVector, FixedModuleVector, NetVector, wirelength);
            gen_cnt++;

            int delta_cost = neighbor_cost - cost;
            bool rand_accept = (double)rand() / RAND_MAX < exp(-1 * (delta_cost) / T);
            if(delta_cost < 0 || rand_accept) {
                if(delta_cost > 0) {
                    uphill_cnt++;
                }
                cost = neighbor_cost;
                if(cost < min_cost) {
                    min_cost = cost;
                    // update best solution
                    best_sol.clear();
                    for(auto it: SoftModuleVector){
                        block b(it->name, it->x, it->y, it->width, it->height);
                        best_sol.push_back(b);
                    }
                    best_wirelength = wirelength;
                }
            } else {
                reject_cnt++;
                // restore SoftModuleVector value and Grid
                restore(changed, Grid);
            }
            
        }

        // Reduce temperature
        T *= T_DECAY;
    }

    // Output

    ofstream MyWriteFile(output_file);
    // Wirelength wirelength
    MyWriteFile << "Wirelength " << best_wirelength << endl;

    MyWriteFile << endl;
    // NumSoftModules number of soft modules
    MyWriteFile << "NumSoftModules " << SoftModuleVector.size() << endl;
    // module name x_coordinate y_coordinate module_width module_height
    for(auto it: best_sol){
        MyWriteFile << it.name << " " << it.x << " " << it.y << " " << it.width << " " << it.height << endl;
    }

    MyWriteFile.close();


}

int main(int argc, char** argv) {
    string input_file = argv[1];
    string output_file = argv[2];
    Parser(input_file, output_file);
    return 0;
}