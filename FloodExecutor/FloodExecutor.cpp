// FloodExecutor.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

#include "pch.h" // Пока пустой заголовочный файл
#include <iostream> // Консольный ввод-вывод
#include <fstream> // Ввод в файл, вывод из файла
#include <string> // Работа со строками
#include <filesystem> // Работа с файлами, например копирование
#include <windows.h> // Системные команды

using namespace std;
namespace fs = experimental::filesystem;

void define_results(string& source)
{
	string Z1, Z2, Z3; // Названия файлов с результатами для одно, двух- и трехзарядной плазм
	int flag = 0;

	Z1 = source + "Z=1 Old Format.dat";
	Z2 = source + "Z=2 Old Format.dat";
	Z3 = source + "Z=3 Old Format.dat";
	if (fs::exists(Z1) && !fs::is_directory(Z1))
	{
		source = Z1;
		flag = 1;
	}
	if (fs::exists(Z2) && !fs::is_directory(Z2))
	{
		source = Z2;
		flag = 1;
	}
	if (fs::exists(Z3) && !fs::is_directory(Z3))
	{
		source = Z3;
		flag = 1;
	}
	if (flag == 0)
		source += ".dat";
}

// Поиск нулевого файла с координатами в директории path
string find_coos(string const& path)
{
	size_t found; // Указатель на найденную подстроку
	string s; // Вспомогательная переменная
	string name; // Полное имя найденного файла

	for (const auto & p : fs::directory_iterator(path))
	{
		s = p.path().string(); // Путь к объекту, преобразованный в строку
		if (!is_directory(p)) // Если файл (не папка)
		{
			// Выделяем имя файла
			found = s.find_last_of("/\\");
			name = s.substr(found + 1);

			found = name.find("coo_");

			if (found != -1) // -1 означает, что подстрока не найдена
			{
				found = name.find("000.dat");
				if (found != -1)
					return name;
			}
		}
	}
	return "error.error";
}

/* 
Найти значение параметра в фалйе
path - путь к файлу
input - название папки, в которой работаем и для которой ищем параметр
search - строка, которую ищем в файле (название параметра)
*/
double find_param(string const& path, string const& input, string const& search)
{
	size_t found; // Указатель на найденную подстроку
	string s; // Вспомогательная переменная

	ifstream readFile(path); // Файл на чтение - ранее обработанные данные
	string readout; // Строка из считываемого файла

	int flag = 0;
	double param = -1;

	while (getline(readFile, readout))
	{
		if (readout.substr(0, search.length()) == search && flag == 1)
		{
			found = readout.find("=") + 1;
			s = readout.substr(found); // Значение параметра в виде строки
			param = stod(s);
			flag = 0;
		}
		// Если в строке находятся не параметры для папки, а название папки (параметры табулируются)
		if (readout.substr(0, 1) != "\t")
		{
			flag = 0;
			// Если искомая папка
			if (readout == (input + ":"))
				flag = 1;
		}
	}

	readFile.close();
	return param;
}

void make_instructions(string const& instructions, string const& param_1, string const& param_2 = "", string const& param_3 = "")
{
	ofstream outFile(instructions);
	outFile << param_1 << endl << param_2 << endl << param_3 << endl;
	outFile.close();
}

void start(string const& prog, string const& arg = "", string const& instructions = "")
{
	string s; // Вспомогательная переменная

	if (instructions == "")
		s = prog + " " + arg;
	else
	{
		if (arg == "")
			s = prog + " < " + instructions;
		else
			s = prog + " " + arg + " < " + instructions;
	}
	//cout << "{" << s << "}" << endl;
	system(s.c_str()); // Строка преобразована в const char *, иначе system не поймет
	// Осуществлен запуск prog с аргументом arg
}

// Переместить файл с заменой
void move(string const& file, string const& path)
{
	if (fs::exists(file))
	{
		if (fs::exists(path + file))
			fs::remove(path + file);
		fs::copy(file, path + file);
		fs::remove(file);
	}
}

// Чистка мусора
// TODO: принимать список файлов на очистку
void clear(string const& f1)
{
	if (fs::exists(f1))
		fs::remove(f1);
}

int process(string const& input, string const& mode)
{
	if (mode != "1" && mode != "2")
	{
		cout << "Invalid mode!" << endl;
		return 1;
	}
	
	// Названия обрабатываемых файлов
	string Q = "Q_l.dat";
	string Q_a = "Qavr.dat";
	string J1 = "flood_Jt.dat";
	string J = "flood_Jt2.dat";
	string f = "flood_out.dat";
	// Название вызываемых программ-обработчиков
	string prog_Q = "flood.exe";
	string prog_Q_a = "average_Q2.exe";
	string prog_J = "flood_Jt2.exe";

	string coos = "error.error"; // Название файла с координатами
	double Te = -1; // Температура электронов
	double Ti = -1; // Температура ионов
	double accuracy = 1; // Допустимое отклонение энергии системы от начального значения (в процентах)
	int slice_step = 1; // Интервал, по которому проводится усреднение (1 = 2 000 шагов по времени)
	int max_change = 1; // Максимально допустимое изменение энергии системы в процентах от начального значения (1 = 1%)
	string instructions = "FE_instructions.dat"; // Название файла с инструкциями к prog_Q
	string source = "Results"; // Название файла с температурами

	string path = "./"; // Текущая папка

	if (!input.empty()) // Не пустая строка
	{
		if (fs::is_directory(input) && fs::exists(input)) // Если папка существует
			path += input + "/";
		else
		{
			cout << endl << "The directory " << input << " doesn't exist" << endl;
			return 0;
		}
	}
	// Если пустая, то считаем, что нужные файлы находятся там же, где эта программа

	cout << endl << "The main directory: " << path << endl;

	coos = find_coos(path);
	// Если так и не нашли
	if (coos == "error.error")
		cout << "The directory " << path << " doesn't have the coordinates dat-file zero!" << endl;
	else
	{
		
		define_results(source);

		if (mode == "1")
		{
			// Извлекаем из файла температуру электронов и записываем в файл с инструкциями
			Te = find_param("./" + source, input, "\tTe=");
			if (Te == -1) // Если темп. не получена (-1 базовое значение)
				cout << "The directory " << path << " doesn't have an electron temperature value!" << endl;
			else
			{
				make_instructions(instructions, to_string(Te), to_string(slice_step), to_string(accuracy));
				cout << endl << "Processing coordinates in the directory " << path << endl;
				cout << "I'm going to input: " << Te << " " << slice_step << " " << accuracy << endl;
			}
		}
		if (mode == "2")
		{
			// FIXME: Повторяющийся участок кода!
			
			// Извлекаем из файла температуру ионов и записываем в файл с инструкциями
			Ti = find_param("./" + source, input, "\tTi=");
			if (Ti == -1) // Если темп. не получена (-1 базовое значение)
				cout << "The directory " << path << " doesn't have an electron temperature value!" << endl;
			else
			{
				make_instructions(instructions, to_string(Ti), to_string(slice_step), to_string(accuracy));
				cout << endl << "Processing coordinates in the directory " << path << endl;
				cout << "I'm going to input: " << Ti << " " << slice_step << " " << accuracy << endl;
			}
		}

		start(prog_Q, path + coos, instructions);
		//start(prog_Q_a);

		move(Q, path);
		move(f, path);
		move(J1, path);
		move(J, path);
	}

	return 0;
}

int main()
{
	string input;
	string mode;
	int from, to; // Номера (они же названия) начальной и конечной папок, которые нужно обработать
	size_t found; // Указатель на найденную подстроку
	string instructions = "FE_instructions.dat"; // Название файла с инструкциями к prog_Q

	string prog_CP = "CopyPaste.exe";
	string prog_S = "Separator.exe";

	// Для вызова CopyPaste.exe и Separator.exe. Для удобства вызова Separator.exe расширения убраны
	string J = "flood_Jt2";
	string Q = "Q_l";
	string file_path;

	cout << "Input a directory name (or a range of directories through '-'): ";
	getline(cin, input); // Запрашиваем название папки, в которой необходимо обработать файлы. getline может принять пустую строку

	cout << "Input a working mode \
\n number 1 - run only flood.exe (Te) and then CopyPaste.exe and Separator.exe, \
\n number 2 - run only flood.exe (Ti) and then CopyPaste.exe and Separator.exe. \n";
	getline(cin, mode);

	found = input.find("-");
	if (found != -1) // Если найдено тире
	{
		from = stoi(input.substr(0, found));
		to = stoi(input.substr(found + 1));

		for (int i = from; i <= to; i++)
			process(to_string(i), mode);
	}
	else
		process(input, mode);

	make_instructions(instructions, "", Q + ".dat");
	start(prog_CP, "", instructions);
			
	file_path = "./" + Q + "/" + prog_S;
	fs::copy(prog_S, file_path);
	start("cd " + Q + " && " + prog_S);
	fs::remove(file_path);

	clear(instructions);
	system("pause");
	return 0;
}